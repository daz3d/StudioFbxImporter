/**********************************************************************
	Copyright (C) 2012-2022 DAZ 3D, Inc. All Rights Reserved.

	Licensed under the Apache License, Version 2.0 (the "License");
	you may not use this file except in compliance with the License.
	You may obtain a copy of the License at

		http://www.apache.org/licenses/LICENSE-2.0

	Unless required by applicable law or agreed to in writing, software
	distributed under the License is distributed on an "AS IS" BASIS,
	WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
	See the License for the specific language governing permissions and
	limitations under the License.
**********************************************************************/

#include "DzFbxImporter.h"

#include <QtGui/QComboBox>

// Public SDK
#include "dzapp.h"
#include "dzbone.h"
#include "dzbonebinding.h"
#include "dzdefaultmaterial.h"
#include "dzenumproperty.h"
#include "dzfacetmesh.h"
#include "dzfacetshape.h"
#include "dzfigure.h"
#include "dzfileiosettings.h"
#include "dzfloatproperty.h"
#include "dzimagemgr.h"
#include "dzmorph.h"
#include "dzmorphdeltas.h"
#include "dznode.h"
#include "dzobject.h"
#include "dzprogress.h"
#include "dzscene.h"
#include "dzsettings.h"
#include "dzskinbinding.h"
#include "dzstyle.h"
#include "dzversion.h"

#if FBXSDK_VERSION_MAJOR >= 2016
#define DATA_FBX_USER_PROPERTIES "FbxUserProperties"
#define DATA_LOD_INFO "LODInfo"
#endif

#if ((DZ_SDK_VERSION_MAJOR >= 5) || ((DZ_SDK_VERSION_MAJOR == 4) && (DZ_SDK_VERSION_MINOR >= 12)))
#define DZ_SDK_4_12_OR_GREATER 1
#else
#define DZ_SDK_4_12_OR_GREATER 0
#endif

namespace
{

const QString c_optionTake( "Take" );
const QString c_optionRunSilent( "RunSilent" );

DzFigure* createFigure()
{
	DzFigure* dsFigure = new DzFigure();

	DzEnumProperty* followModeControl = NULL;
#if DZ_SDK_4_12_OR_GREATER
	followModeControl = dsFigure->getFollowModeControl();
	followModeControl->setValue( DzSkeleton::fmAutoFollow );
#else
	// DzSkeleton::getFollowModeControl() is not in the 4.5 SDK, so we attempt
	// to use the meta-object to call the methods. If this fails, we attempt to
	// find the property by name and if found set its value.

	if ( !QMetaObject::invokeMethod( dsFigure,
		"getFollowModeControl", Q_RETURN_ARG( DzEnumProperty*, followModeControl ) ) 
		|| !followModeControl )
	{
		followModeControl = qobject_cast<DzEnumProperty*>( dsFigure->findProperty( "Fit to Mode" ) );
		if ( followModeControl )
		{
			followModeControl->setValue( 1 );
		}
	}
#endif

	return dsFigure;
}

} // namespace

///////////////////////////////////////////////////////////////////////
// DzFbxImporter
///////////////////////////////////////////////////////////////////////

#ifdef DAZ_SCRIPT_DOCS

class DzFbxImporter : public DzImporter {
public:
// DAZScript Constructors

	DzFbxImporter();
};

#endif // DAZ_SCRIPT_DOCS

/**
	@script
	@class	DzFbxImporter

	@ingroup	FileIO
	@nosubgrouping
	@brief		Class for importing files in the Autodesk FBX (*.fbx) format.

	A specialization of DzImporter that implements an importer for the Autodesk
	FBX (.fbx) format. Through its use of the FBX SDK to accomplish this task,
	this importer also provides import capabilities for the Autodesk AutoCAD DXF
	(.dxf) format, the Autodesk 3ds Max (.3ds) format, and the Collada DAE (.dae)
	format, as supported by the FBX SDK.

	@attention	The FBX SDK also provides import capabilities for the Alias
				Wavefront OBJ (.obj) format, but it has been intentionally
				excluded. Use DzObjImporter instead.

	@sa DzImportMgr::findImporterByClassName()
	@sa DzImportMgr::findImporter()

	@sa Sample: @dzscript{api_reference/samples/file_io/import_fbx_silent/start, Silent FBX Import}
**/


/**
	@script
	Default Constructor.
**/
DzFbxImporter::DzFbxImporter() :
	m_fbxManager( NULL ),
	m_fbxScene( NULL ),
	m_fbxAnimStack( NULL ),
	m_fbxAnimLayer( NULL ),
	m_needConversion( false ),
	m_dsEndTime( 0 ),
	m_rigErrorPre( false ),
	m_rigErrorSkin( false ),
	m_rigErrorScale( false ),
	m_rigErrorRoot( false ),
	m_suppressRigErrors( false ),
	m_root( NULL )
{
}

/**
**/
DzFbxImporter::~DzFbxImporter()
{
}

/**
**/
bool DzFbxImporter::recognize( const QString &filename ) const
{
	const QString ext = getFileExtension( filename ); //return value is lowercase
	for ( int i = 0, n = getNumExtensions(); i < n; i++ )
	{
		if ( ext == getExtension( i ) )
		{
			return true;
		}
	}

	return false;
}

/**
**/
int DzFbxImporter::getNumExtensions() const
{
	return 4;
}

/**
**/
QString	DzFbxImporter::getExtension( int i ) const
{
	if ( i == 0 )
	{
		return "fbx";
	}

	if ( i == 1 )
	{
		return "dxf";
	}

	if ( i == 2 )
	{
		return "3ds";
	}

	if ( i == 3 )
	{
		return "dae";
	}

	/*
	if ( i == 4 )
	{
		return "obj";
	}
	*/

	return QString();
}

/**
**/
QString	DzFbxImporter::getDescription() const
{
	return "Autodesk FBX";
}

/**
**/
void DzFbxImporter::getDefaultOptions( DzFileIOSettings* options ) const
{
	assert( options );
	if ( !options )
	{
		return;
	}

	options->setStringValue( c_optionTake, QString() );
	options->setIntValue( c_optionRunSilent, 0 );
}


namespace
{

class MapConversion {
public:
	QVector<double> fbxWeights;
	unsigned short* dsWeights;
};

bool isChildNode( DzNode* child, DzNode* parent )
{
	if ( !parent || !child || child == parent )
	{
		return false;
	}

	while ( child )
	{
		if ( child == parent )
		{
			return true;
		}
		child = child->getNodeParent();
	}

	return false;
}

} //namespace

/**
	Manually get the options. If the "RunSilent" option is true, then the dialog
	will be skipped.
**/
int DzFbxImporter::getOptions( DzFileIOSettings* options, const DzFileIOSettings* impOptions, const QString &filename )
{
	assert( options );
	assert( impOptions );
	if ( !options || !impOptions )
	{
		return false;
	}

	bool optionsShown = false;
#if DZ_SDK_4_12_OR_GREATER
	optionsShown = getOptionsShown();
#else
	// DzFileIO::getOptionsShown() is not in the 4.5 SDK, so we attempt
	// to use the meta-object to call the method.

	QMetaObject::invokeMethod( this,
		"getOptionsShown", Q_RETURN_ARG( bool, optionsShown ) );
#endif

	if ( optionsShown || impOptions->getIntValue( c_optionRunSilent, 0 ) )
	{
		if ( optionsShown )
		{
			getSavedOptions( options ); // includes defaults
		}
		else
		{
			getDefaultOptions( options );
		}

#if DZ_SDK_4_12_OR_GREATER
		copySettings( options, impOptions );
#else
		// DzFileIO::copySettings() is not in the 4.5 SDK, so we attempt
		// to use the meta-object to call the method.

		QMetaObject::invokeMethod( this,
			"copySettings", Q_ARG( DzFileIOSettings*, options ), Q_ARG( const DzFileIOSettings*, impOptions ) );
#endif

		return true;
	}


	QString errors;
	for ( int i = 0; i < m_fbxScene->GetRootNode()->GetChildCount(); i++ )
	{
		fbxPreRecurse( m_fbxScene->GetRootNode()->GetChild( i ) );
	}

	if ( !m_suppressRigErrors )
	{
		if ( m_rigErrorRoot )
		{
			errors += "Rigging limitation: bones without root skeleton.\n";
		}

		if ( m_rigErrorSkin )
		{
			errors += "Rigging limitation: cluster links reference non bone.\n";
		}

		if ( m_rigErrorPre )
		{
			errors += "Rigging limitation: pre and post rotation must match in current implementation.\n";
		}

		if ( m_rigErrorScale )
		{
			errors += "Transform differences: non uniform scale detected.  Results will likely be different.\n";
		}
	}

	QStringList animStackNames;

	for ( int i = 0; i < m_fbxScene->GetSrcObjectCount<FbxAnimStack>(); i++ )
	{
		const FbxAnimStack* animStack = m_fbxScene->GetSrcObject<FbxAnimStack>( i );
		const int numLayers = animStack->GetMemberCount<FbxAnimLayer>();

		QString error;
		if ( numLayers == 0 )
		{
			errors += "Unexpected: " % QString( animStack->GetName() ) % " has no layers \n";
		}
		else if ( numLayers > 1 )
		{
			errors += "Animation Limitation: " % QString( animStack->GetName() ) % " has multiple layers \n";
		}

		animStackNames.push_back( QString( animStack->GetName() ) );
	}


	DzFbxImporter* self = const_cast<DzFbxImporter*>( this );
	DzFbxImportFrame* frame = new DzFbxImportFrame( self, animStackNames, errors );
	if ( !frame )
	{
		return true;
	}

	DzFileIODlg optionsDlg( frame );
	frame->setOptions( impOptions, filename );

	if ( animStackNames.size() > 1 || errors.size() )
	{
		if ( optionsDlg.exec() != QDialog::Accepted )
		{
			return false; // user cancelled
		}
	}

#if DZ_SDK_4_12_OR_GREATER
	setOptionsShown( true );
#else
	// DzFileIO::setOptionsShown() is not in the 4.5 SDK, so we attempt
	// to use the meta-object to call the method.

	QMetaObject::invokeMethod( this, "setOptionsShown", Q_ARG( bool, true ) );
#endif

	frame->getOptions( options );

	// if handling the options dialog ourselves, we also need to save the state
	options->setIntValue( c_optionRunSilent, 0 );
	saveOptions( options );

	return true;
}

/**
	@param filename		The full path of the file to import.
	@param impOptions	The options to use while importing the file.

	@return	DZ_NO_ERROR if the file was successfully imported.
**/
DzError DzFbxImporter::read( const QString &filename, const DzFileIOSettings* impOptions )
{
#if DZ_SDK_4_12_OR_GREATER
	clearImportedNodes();
#else
	// DzImporter::clearImportedNodes() is not in the 4.5 SDK, so we attempt
	// to use the meta-object to call the method.

	QMetaObject::invokeMethod( this, "clearImportedNodes" );
#endif

	DzProgress progress( "Importing" );

	m_folder = filename;
	m_folder.cdUp();

	QString orgName;
#if DZ_SDK_4_12_OR_GREATER
	orgName = dzApp->getOrgName();
#else
	// DzApp::getOrgName() is not in the 4.5 SDK, so we attempt to use the
	// meta-object to call the method.

	QMetaObject::invokeMethod( dzApp,
		"getOrgName", Q_RETURN_ARG( QString, orgName ) );
#endif
	if ( !orgName.isEmpty() && orgName != QString( "DAZ 3D" ) )
	{
		m_suppressRigErrors = true;
	}

	m_fbxManager = FbxManager::Create();
	FbxIOSettings* fbxIoSettings = FbxIOSettings::Create( m_fbxManager, IOSROOT );
	m_fbxManager->SetIOSettings( fbxIoSettings );
	m_fbxScene = FbxScene::Create( m_fbxManager, "" );
	m_fbxAnimStack = 0;
	m_fbxAnimLayer = 0;
	m_dsEndTime = dzScene->getAnimRange().getEnd();

	FbxImporter* fbxImporter = FbxImporter::Create( m_fbxManager, "" );
	fbxImporter->Initialize( filename.toLatin1().data(), -1, fbxIoSettings );

	if ( fbxImporter->IsFBX() )
	{
		fbxIoSettings->SetBoolProp( IMP_FBX_MATERIAL,        true );
		fbxIoSettings->SetBoolProp( IMP_FBX_TEXTURE,         true );
		fbxIoSettings->SetBoolProp( IMP_FBX_LINK,            true );
		fbxIoSettings->SetBoolProp( IMP_FBX_SHAPE,           true );
		fbxIoSettings->SetBoolProp( IMP_FBX_GOBO,            true );
		fbxIoSettings->SetBoolProp( IMP_FBX_ANIMATION,       true );
		fbxIoSettings->SetBoolProp( IMP_FBX_GLOBAL_SETTINGS, true );
	}

	fbxImporter->Import( m_fbxScene );
	fbxImporter->Destroy();

	QScopedPointer<DzFileIOSettings> options( new DzFileIOSettings() );
	const int isOK = getOptions( options.data(), impOptions, filename );
	if ( !isOK )
	{
		return DZ_USER_CANCELLED_OPERATION;
	}

	const QString takeName = options->getStringValue( c_optionTake );

	for ( int i = 0; i < m_fbxScene->GetSrcObjectCount<FbxAnimStack>(); i++ )
	{
		const FbxAnimStack* animStack = m_fbxScene->GetSrcObject<FbxAnimStack>( i );
		if ( QString( animStack->GetName() ) == takeName )
		{
			m_fbxAnimStack = m_fbxScene->GetSrcObject<FbxAnimStack>( i );

			const int numLayers = m_fbxAnimStack->GetMemberCount<FbxAnimLayer>();
			if ( numLayers > 0 )
			{
				m_fbxAnimLayer = m_fbxAnimStack->GetMember<FbxAnimLayer>( 0 );
			}
		}
	}

	//if ( false )
	{
		m_root = new Node();
		m_root->fbxNode = m_fbxScene->GetRootNode();
		fbxImportGraph( m_root );

		QVector<DzBone*> dsBones;
		for ( int i = 0; i < m_skins.size(); i++ )
		{
			const Node* node = m_skins[i].node;
			Q_UNUSED( node )
			FbxSkin* fbxSkin = m_skins[i].fbxSkin;
			DzFigure* dsFigure = m_skins[i].dsFigure;
			const int numVertices = m_skins[i].numVertices;

			DzSkinBinding* dsSkin = dsFigure->getSkinBinding();

#if DZ_SDK_4_12_OR_GREATER
			if ( dsSkin
				&& dsSkin->getTargetVertexCount() < 1 )
			{
				dsSkin->setTargetVertexCount( numVertices );
#else
			// DzSkinBinding::getTargetVertexCount() is not in the 4.5 SDK, so
			// we attempt to use the meta-object to call the method.
			int targetVertexCount = 0;
			if ( dsSkin
				&& QMetaObject::invokeMethod( dsSkin,
				"getTargetVertexCount", Q_RETURN_ARG( int, targetVertexCount ) )
				&& targetVertexCount < 1 )
			{
				// DzSkinBinding::setTargetVertexCount() is not in the 4.5 SDK,
				// so we attempt to use the meta-object to call the method.

				QMetaObject::invokeMethod( dsSkin,
					"setTargetVertexCount", Q_ARG( int, numVertices ) );
#endif
			}
			else if ( !dsSkin )
			{
				assert( !"Binding was not found" );
				continue;
			}

			dsFigure->setDrawGLBones( true );

			DzSkeleton* crossSkeleton = NULL;
			for ( int j = 0; j < fbxSkin->GetClusterCount(); j++ )
			{
				FbxCluster* fbxCluster = fbxSkin->GetCluster( j );
				DzBone* dsBone = qobject_cast<DzBone*>( m_nodeMap[fbxCluster->GetLink()] );
				if ( !dsBone )
				{
					continue;
				}

				if ( !isChildNode( dsBone, dsFigure ) )
				{
					crossSkeleton = dsBone->getSkeleton();
				}
			}

			if ( crossSkeleton )
			{
				replicateSkeleton( crossSkeleton, m_skins[i] );
			}


			DzWeightMapList maps;
			QVector<MapConversion> mapConversions;
			for ( int j = 0; j < fbxSkin->GetClusterCount(); j++ )
			{
				FbxCluster* fbxCluster = fbxSkin->GetCluster( j );
				DzBone* dsBone = qobject_cast<DzBone*>( m_nodeMap[fbxCluster->GetLink()] );
				if ( !dsBone )
				{
					continue;
				}

				DzBoneBinding* dsBinding = new DzBoneBinding();
				dsBinding->setBone( dsBone );
				dsSkin->addBoneBinding( dsBinding );

				DzWeightMap* dsWeightMap  = new DzWeightMap( numVertices );
				int* fbxIndices = fbxCluster->GetControlPointIndices();
				double* fbxWeights = fbxCluster->GetControlPointWeights();

				MapConversion mapConv;
				mapConv.dsWeights = dsWeightMap->getWeights();
				mapConv.fbxWeights.resize( numVertices );
				for ( int k = 0; k < fbxCluster->GetControlPointIndicesCount(); k++ )
				{
					mapConv.fbxWeights[fbxIndices[k]] = fbxWeights[k];
				}
				mapConversions.append( mapConv );

				dsBinding->setWeights( dsWeightMap );
				FbxAMatrix fbxMatrix;
				fbxCluster->GetTransformLinkMatrix( fbxMatrix );

				DzMatrix3 dsMatrix;
				dsMatrix[0][0] =  fbxMatrix[0][0];
				dsMatrix[0][1] =  fbxMatrix[0][1];
				dsMatrix[0][2] =  fbxMatrix[0][2];
				dsMatrix[1][0] =  fbxMatrix[1][0];
				dsMatrix[1][1] =  fbxMatrix[1][1];
				dsMatrix[1][2] =  fbxMatrix[1][2];
				dsMatrix[2][0] =  fbxMatrix[2][0];
				dsMatrix[2][1] =  fbxMatrix[2][1];
				dsMatrix[2][2] =  fbxMatrix[2][2];
				dsMatrix[3][0] = -fbxMatrix[3][0];
				dsMatrix[3][1] = -fbxMatrix[3][1];
				dsMatrix[3][2] = -fbxMatrix[3][2];

				DzVec3 skelOrigin = dsFigure->getOrigin();
				DzVec3 origin = dsBone->getOrigin();
				dsMatrix[3][0] += ( origin[0] - skelOrigin[0] );
				dsMatrix[3][1] += ( origin[1] - skelOrigin[1] );
				dsMatrix[3][2] += ( origin[2] - skelOrigin[2] );
				dsBinding->setBindingMatrix( dsMatrix );
				maps.append( dsWeightMap );
			}

			for ( int v = 0; v < numVertices; v++ )
			{
				double sum = 0.0;
				for ( int m = 0; m < mapConversions.count(); m++ )
				{
					sum += mapConversions[m].fbxWeights[v];
				}

				for ( int m = 0; m < mapConversions.count(); m++ )
				{
					mapConversions[m].dsWeights[v] = static_cast<unsigned short>( mapConversions[m].fbxWeights[v] / sum * DZ_USHORT_MAX );
				}
			}

			DzWeightMap::normalizeMaps( maps );

#if DZ_SDK_4_12_OR_GREATER
			dsSkin->setBindingMode( DzSkinBinding::General );
			dsSkin->setScaleMode( DzSkinBinding::BindingMaps );
#else
			// DzSkinBinding::setBindingMode() and DzSkinBinding::setScaleMode()
			// are not in the 4.5 SDK, so we attempt to use the meta-object to
			// call these methods.

			QMetaObject::invokeMethod( dsSkin,
				"setBindingMode", Q_ARG( int, 0 ) );

			QMetaObject::invokeMethod( dsSkin,
				"setScaleMode", Q_ARG( int, 1 ) );
#endif
		}

		fbxImportAnim( m_root );

		dzScene->setAnimRange( DzTimeRange( dzScene->getAnimRange().getStart(), m_dsEndTime ) );
		dzScene->setPlayRange( DzTimeRange( dzScene->getAnimRange().getStart(), m_dsEndTime ) );
	}
	m_fbxManager->Destroy();
	m_fbxManager = 0;


	bool allTransparent = true;
	for ( int i = 0; i < m_dsMaterials.size() && allTransparent; i++ )
	{
		if ( m_dsMaterials[i]->getBaseOpacity() > 0.1 )
		{
			allTransparent = false;
		}
	}

	if ( allTransparent )
	{
		for ( int i = 0; i < m_dsMaterials.size() && allTransparent; i++ )
		{
			m_dsMaterials[i]->setBaseOpacity( 1 );
		}
	}

	return DZ_NO_ERROR;
}

/**
**/
void DzFbxImporter::replicateSkeleton( DzSkeleton* crossSkeleton, Skinning &skinning )
{
	Node* node = skinning.node;
	DzSkeleton* dsSkeleton = qobject_cast<DzSkeleton*>( node->dsNode );

	if ( !dsSkeleton )
	{
		return;
	}

	Node* crossNode = m_root->find( crossSkeleton );

	for ( int i = 0; i < crossNode->children.count(); i++ )
	{
		Node* crossChild = crossNode->children[i];
		if ( crossChild->fbxNode->GetNodeAttribute() && crossChild->fbxNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			continue;
		}

		Node* child = new Node();
		child->setParent( node );
		child->fbxNode = crossChild->fbxNode;
		child->dsParent = node->dsNode;

		fbxImportGraph( child );
	}

	dsSkeleton->setFollowTarget( crossSkeleton );
}

static DzVec3 toVec3( FbxVector4 v )
{
	DzVec3 r;
	r.m_x = v[0];
	r.m_y = v[1];
	r.m_z = v[2];
	r.m_w = v[3];
	return r;
}

static FbxVector4 calcFbxRotationOffset( FbxNode* fbxNode )
{
	FbxVector4 offset( 0, 0, 0 );
	while ( fbxNode )
	{
		bool applyOffset = true;

		if ( fbxNode->GetNodeAttribute() && fbxNode->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh )
		{
			applyOffset = false;
		}

		if ( applyOffset )
		{
			offset += fbxNode->GetRotationOffset( FbxNode::eSourcePivot );
		}

		fbxNode = fbxNode->GetParent();
	}
	offset[3] = 1;
	return offset;
}

static void setNodeOrientation( DzNode* dsNode, FbxNode* fbxNode )
{
	FbxVector4 fbxPre = fbxNode->GetPreRotation( FbxNode::eSourcePivot );
	const DzQuat rot( DzRotationOrder::XYZ, DzVec3( fbxPre[0], fbxPre[1], fbxPre[2] ) * DZ_FLT_DEG_TO_RAD );

	dsNode->setOrientation( rot, true );
}

static void setNodeRotationOrder( DzNode* dsNode, FbxNode* fbxNode )
{
	DzRotationOrder dsRotationOrder( DzRotationOrder::XYZ );
	EFbxRotationOrder fbxRotationOrder( eEulerXYZ );

	fbxNode->GetRotationOrder( FbxNode::eSourcePivot, fbxRotationOrder );
	switch ( fbxRotationOrder )
	{
	case eEulerXYZ:
		dsRotationOrder = DzRotationOrder::XYZ;
		break;
	case eEulerXZY:
		dsRotationOrder = DzRotationOrder::XZY;
		break;
	case eEulerYXZ:
		dsRotationOrder = DzRotationOrder::YXZ;
		break;
	case eEulerYZX:
		dsRotationOrder = DzRotationOrder::YZX;
		break;
	case eEulerZXY:
		dsRotationOrder = DzRotationOrder::ZXY;
		break;
	case eEulerZYX:
		dsRotationOrder = DzRotationOrder::ZYX;
		break;
	default:
		break;
	}

	dsNode->setRotationOrder( dsRotationOrder );
}

static bool _allClose( double a, double b, double c )
{
	if ( qAbs( a - b ) > 0.0000000001f || qAbs( a - c ) > 0.0000000001f )
	{
		return false;
	}

	return true;
}

/**
**/
void DzFbxImporter::fbxPreRecurse( FbxNode* fbxNode )
{
	if ( fbxNode->GetPreRotation( FbxNode::eSourcePivot ) != fbxNode->GetPostRotation( FbxNode::eSourcePivot ) )
	{
		m_rigErrorPre = true;
	}

	if ( !_allClose( fbxNode->LclScaling.Get()[0], fbxNode->LclScaling.Get()[1], fbxNode->LclScaling.Get()[2] ) )
	{
		m_rigErrorScale = true;
	}

	if ( const FbxMesh* fbxMesh = fbxNode->GetMesh() )
	{
		for ( int i = 0; i < fbxMesh->GetDeformerCount(); i++ )
		{
			FbxDeformer* deformer = fbxMesh->GetDeformer( i );
			if ( !deformer->GetClassId().Is( FbxSkin::ClassId ) )
			{
				continue;
			}

			FbxSkin* fbxSkin = static_cast< FbxSkin* >( deformer );
			for ( int j = 0; j < fbxSkin->GetClusterCount(); j++ )
			{
				FbxNode* fbxClusterNode = fbxSkin->GetCluster( j )->GetLink();
				if ( !fbxClusterNode->GetNodeAttribute() ||
					fbxClusterNode->GetNodeAttribute()->GetAttributeType() != FbxNodeAttribute::eSkeleton )
				{
					m_rigErrorSkin = true;
				}
			}
		}
	}

	if ( const FbxSkeleton* fbxSkeleton = fbxNode->GetSkeleton() )
	{
		if ( fbxSkeleton->GetSkeletonType() != FbxSkeleton::eRoot )
		{
			const FbxSkeleton* fbxParentSkeleton = fbxNode->GetParent()->GetSkeleton();
			if ( !fbxParentSkeleton )
			{
				m_rigErrorRoot = true;
			}
		}
	}

	for ( int i = 0; i < fbxNode->GetChildCount(); i++ )
	{
		fbxPreRecurse( fbxNode->GetChild( i ) );
	}
}

/**
**/
void DzFbxImporter::fbxImportGraph( Node* node )
{
	DzNode* dsMeshNode = NULL;

	if ( node == m_root )
	{
		for ( int i = 0; i < node->fbxNode->GetChildCount(); i++ )
		{
			Node* child = new Node();
			child->setParent( node );
			child->fbxNode = node->fbxNode->GetChild( i );
		}

		for ( int i = 0; i < node->children.count(); i++ )
		{
			fbxImportGraph( node->children[i] );
		}

		return;
	}

	const FbxNull* fbxNull = node->fbxNode->GetNull();
	if ( fbxNull || !node->fbxNode->GetNodeAttribute() )
	{
		node->dsNode = new DzNode();
	}
	else
	{
		switch ( node->fbxNode->GetNodeAttribute()->GetAttributeType() )
		{
		case FbxNodeAttribute::eMarker:
			break;
		case FbxNodeAttribute::eSkeleton:
			{
				const FbxSkeleton* fbxSkeleton = node->fbxNode->GetSkeleton();
				switch ( fbxSkeleton->GetSkeletonType() )
				{
				case FbxSkeleton::eRoot:
					node->dsNode = createFigure();
					break;
				case FbxSkeleton::eLimb:
					node->dsNode = new DzBone();
					node->dsNode->setInheritScale( true );
					break;
				case FbxSkeleton::eLimbNode:
					node->dsNode = new DzBone();
					node->dsNode->setInheritScale( true );
					break;
				case FbxSkeleton::eEffector:
					break;
				}
			}
			break;
		case FbxNodeAttribute::eMesh:
			{
				const FbxMesh* fbxMesh = node->fbxNode->GetMesh();
				bool hasSkin = false;
				for ( int i = 0; i < fbxMesh->GetDeformerCount(); i++ )
				{
					const FbxDeformer* deformer = fbxMesh->GetDeformer( i );
					if ( deformer->GetClassId().Is( FbxSkin::ClassId ) )
					{
						hasSkin = true;
						break;
					}
				}

				const QString fbxNodeName( node->fbxNode->GetName() );
				if ( node->dsParent &&
					!node->dsParent->getObject() &&
					( node->dsParent->getName() + ".Shape" == fbxNodeName ||
						node->dsParent->getName() + "_Shape" == fbxNodeName ) )
				{
					dsMeshNode = node->dsParent;
				}
				else
				{
					if ( hasSkin )
					{
						node->dsNode = createFigure();
						node->collapseTranslation = true;
					}
					else
					{
						node->dsNode = new DzNode();
					}

					dsMeshNode = node->dsNode;
				}

				fbxImportMesh( node, node->fbxNode, dsMeshNode );
			}
			break;
		case FbxNodeAttribute::eNurbs:
			break;
		case FbxNodeAttribute::ePatch:
			break;
		case FbxNodeAttribute::eCamera:
			break;
		case FbxNodeAttribute::eLight:
			break;
		case FbxNodeAttribute::eLODGroup:
			break;
		default:
			FbxNodeAttribute::EType type = node->fbxNode->GetNodeAttribute()->GetAttributeType();
			break;
		}
	}

	if ( node->dsNode )
	{
		m_nodeMap[node->fbxNode] = node->dsNode;

		node->dsNode->setName( node->fbxNode->GetName() );
		if ( node->dsParent )
		{
			node->dsParent->addNodeChild( node->dsNode );
		}

		FbxTransform::EInheritType inheritType;
		node->fbxNode->GetTransformationInheritType( inheritType );
		if ( inheritType == FbxTransform::eInheritRrs )
		{
			node->dsNode->setInheritScale( false );
		}
		else
		{
			node->dsNode->setInheritScale( true );
		}

		const FbxVector4 rotationOffset = calcFbxRotationOffset( node->fbxNode );
		node->dsNode->setOrigin( toVec3( rotationOffset ), true );
		setNodeOrientation( node->dsNode, node->fbxNode );
		setNodeRotationOrder( node->dsNode, node->fbxNode );

		if ( rotationOffset.SquareLength() == 0.0 )
		{
			bool found = false;
			FbxMatrix fbxMatrix;

			for ( int i = 0; i < m_fbxScene->GetPoseCount(); i++ )
			{
				const FbxPose* pose = m_fbxScene->GetPose( i );
				if ( pose->IsBindPose() )
				{
					for ( int j = 0; j < pose->GetCount(); j++ )
					{
						if ( pose->GetNode( j ) == node->fbxNode )
						{
							found = true;
							fbxMatrix = pose->GetMatrix( j );
						}
					}
				}
			}

			if ( !found )
			{
				fbxMatrix = node->fbxNode->EvaluateGlobalTransform();
			}

			node->bindTranslation[0] = fbxMatrix[3][0];
			node->bindTranslation[1] = fbxMatrix[3][1];
			node->bindTranslation[2] = fbxMatrix[3][2];
			if ( !node->collapseTranslation )
			{
				node->dsNode->setOrigin( node->bindTranslation, true );
			}
		}

		if ( node->collapseTranslation && dsMeshNode )
		{
			DzFacetMesh* dsFacetMesh = qobject_cast<DzFacetMesh*>( dsMeshNode->getObject()->getCurrentShape()->getGeometry() );
			if ( dsFacetMesh )
			{
				DzPnt3* vertices = dsFacetMesh->getVerticesPtr();
				for ( int i = 0; i < dsFacetMesh->getNumVertices(); i++ )
				{
					vertices[i][0] += node->bindTranslation[0];
					vertices[i][1] += node->bindTranslation[1];
					vertices[i][2] += node->bindTranslation[2];
				}
			}
		}

		dzScene->addNode( node->dsNode );

#if DZ_SDK_4_12_OR_GREATER
		addImportedNode( node->dsNode );
#else
		// DzImporter::addImportedNode() is not in the 4.5 SDK, so we attempt to
		// use the meta-object to call the method.

		QMetaObject::invokeMethod( this,
			"addImportedNode", Q_ARG( DzNode*, node->dsNode ) );
#endif

		for ( int i = 0; i < node->fbxNode->GetChildCount(); i++ )
		{
			Node* child = new Node();
			child->setParent( node );
			child->dsParent = node->dsNode;
			child->fbxNode = node->fbxNode->GetChild( i );
		}

		for ( int i = 0; i < node->children.count(); i++ )
		{
			fbxImportGraph( node->children[i] );
		}

		DzVec3 endPoint = node->dsNode->getOrigin();
		if ( node->dsNode->getNumNodeChildren() )
		{
			endPoint = node->dsNode->getNodeChild( 0 )->getOrigin( true );
			for ( int i = 1; i < node->dsNode->getNumNodeChildren(); i++ )
			{
				endPoint += node->dsNode->getNodeChild( i )->getOrigin( true );
			}
			endPoint.m_x /= node->dsNode->getNumNodeChildren();
			endPoint.m_y /= node->dsNode->getNumNodeChildren();
			endPoint.m_z /= node->dsNode->getNumNodeChildren();
			node->dsNode->setEndPoint( endPoint, true );
		}
		else
		{
			DzVec3 toCenter = endPoint;
			if ( node->dsNode->getNodeParent() )
			{
				toCenter -= node->dsNode->getNodeParent()->getOrigin();
				toCenter = ( node->dsNode->getNodeParent()->getOrientation().inverse() * node->dsNode->getOrientation() ).multVec( toCenter );
				if ( toCenter.length() > 1 )
				{
					toCenter /= 2.0;
				}
			}
			node->dsNode->setEndPoint( endPoint + toCenter, true );
		}

		// setup a decent guess so IK will work
		if ( qobject_cast<DzBone*>( node->dsNode ) &&
			qobject_cast<DzBone*>( node->dsParent ) &&
			node->dsNode->getNumNodeChildren() != 0 )
		{
			node->dsNode->getXPosControl()->setHidden( true );
			node->dsNode->getYPosControl()->setHidden( true );
			node->dsNode->getZPosControl()->setHidden( true );
		}

		if ( node->fbxNode->GetRotationLimits().GetActive() )
		{
			FbxDouble3 min = node->fbxNode->GetRotationLimits().GetMin();
			FbxDouble3 max = node->fbxNode->GetRotationLimits().GetMax();

			if ( node->fbxNode->GetRotationLimits().GetMaxXActive() &&
				node->fbxNode->GetRotationLimits().GetMinXActive() )
			{
				node->dsNode->getXRotControl()->setIsClamped( true );
				node->dsNode->getXRotControl()->setMin( min[0] );
				node->dsNode->getXRotControl()->setMax( max[0] );
			}

			if ( node->fbxNode->GetRotationLimits().GetMaxYActive() &&
				node->fbxNode->GetRotationLimits().GetMinYActive() )
			{
				node->dsNode->getYRotControl()->setIsClamped( true );
				node->dsNode->getYRotControl()->setMin( min[1] );
				node->dsNode->getYRotControl()->setMax( max[1] );
			}

			if ( node->fbxNode->GetRotationLimits().GetMaxZActive() &&
				node->fbxNode->GetRotationLimits().GetMinZActive() )
			{
				node->dsNode->getZRotControl()->setIsClamped( true );
				node->dsNode->getZRotControl()->setMin( min[2] );
				node->dsNode->getZRotControl()->setMax( max[2] );
			}
		}
	}
}

/**
**/
void DzFbxImporter::fbxImportSkin( Node* node )
{
	for ( int i = 0; i < node->children.count(); i++ )
	{
		fbxImportSkin( node->children[i] );
	}
}

/**
**/
void DzFbxImporter::fbxImportAnim( Node* node )
{
	if ( node->dsNode )
	{
		DzVec3 translationOffset = node->bindTranslation;
		if ( node->parent && !node->parent->collapseTranslation )
		{
			translationOffset -= node->parent->bindTranslation;
		}

		const float posX = node->fbxNode->LclTranslation.Get()[0] - translationOffset[0];
		const float posY = node->fbxNode->LclTranslation.Get()[1] - translationOffset[1];
		const float posZ = node->fbxNode->LclTranslation.Get()[2] - translationOffset[2];

		if ( !node->collapseTranslation )
		{
			node->dsNode->getXPosControl()->setDefaultValue( posX );
			node->dsNode->getYPosControl()->setDefaultValue( posY );
			node->dsNode->getZPosControl()->setDefaultValue( posZ );
			node->dsNode->getXPosControl()->setValue( posX );
			node->dsNode->getYPosControl()->setValue( posY );
			node->dsNode->getZPosControl()->setValue( posZ );

			node->dsNode->getXRotControl()->setDefaultValue( node->fbxNode->LclRotation.Get()[0] );
			node->dsNode->getYRotControl()->setDefaultValue( node->fbxNode->LclRotation.Get()[1] );
			node->dsNode->getZRotControl()->setDefaultValue( node->fbxNode->LclRotation.Get()[2] );
			node->dsNode->getXRotControl()->setValue( node->fbxNode->LclRotation.Get()[0] );
			node->dsNode->getYRotControl()->setValue( node->fbxNode->LclRotation.Get()[1] );
			node->dsNode->getZRotControl()->setValue( node->fbxNode->LclRotation.Get()[2] );

			node->dsNode->getXScaleControl()->setDefaultValue( node->fbxNode->LclScaling.Get()[0] );
			node->dsNode->getYScaleControl()->setDefaultValue( node->fbxNode->LclScaling.Get()[1] );
			node->dsNode->getZScaleControl()->setDefaultValue( node->fbxNode->LclScaling.Get()[2] );
			node->dsNode->getXScaleControl()->setValue( node->fbxNode->LclScaling.Get()[0] );
			node->dsNode->getYScaleControl()->setValue( node->fbxNode->LclScaling.Get()[1] );
			node->dsNode->getZScaleControl()->setValue( node->fbxNode->LclScaling.Get()[2] );
		}

		if ( m_fbxAnimLayer && !node->collapseTranslation )
		{
			applyFbxCurve( node->fbxNode->LclTranslation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_X ), node->dsNode->getXPosControl() );
			applyFbxCurve( node->fbxNode->LclTranslation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y ), node->dsNode->getYPosControl() );
			applyFbxCurve( node->fbxNode->LclTranslation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z ), node->dsNode->getZPosControl() );

			applyFbxCurve( node->fbxNode->LclRotation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_X ), node->dsNode->getXRotControl() );
			applyFbxCurve( node->fbxNode->LclRotation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y ), node->dsNode->getYRotControl() );
			applyFbxCurve( node->fbxNode->LclRotation.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z ), node->dsNode->getZRotControl() );

			applyFbxCurve( node->fbxNode->LclScaling.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_X ), node->dsNode->getXScaleControl() );
			applyFbxCurve( node->fbxNode->LclScaling.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Y ), node->dsNode->getYScaleControl() );
			applyFbxCurve( node->fbxNode->LclScaling.GetCurve( m_fbxAnimLayer, FBXSDK_CURVENODE_COMPONENT_Z ), node->dsNode->getZScaleControl() );
		}


#if FBXSDK_VERSION_MAJOR >= 2016
		for ( FbxProperty prop = node->fbxNode->GetFirstProperty(); prop.IsValid(); prop = node->fbxNode->GetNextProperty( prop ) )
		{
			if ( !prop.GetFlag( FbxPropertyFlags::eUserDefined ) )
			{
				continue;
			}

			if ( node->dsNode->findDataItem( DATA_FBX_USER_PROPERTIES ) == NULL )
			{
				node->dsNode->addDataItem( new DzSimpleElementData( DATA_FBX_USER_PROPERTIES, true ) );
			}

			auto key = prop.GetName();
			auto data = qobject_cast<DzSimpleElementData*>( node->dsNode->findDataItem( DATA_FBX_USER_PROPERTIES ) );
			auto settings = data->getSettings();

			auto type = prop.GetPropertyDataType();
			switch ( type.GetType() )
			{
			case eFbxInt:
				settings->setIntValue( QString( key ), prop.Get<int>() );
				break;
			case eFbxBool:
				settings->setBoolValue( QString( key ), prop.Get<bool>() );
				break;
			case eFbxFloat:
				settings->setFloatValue( QString( key ), prop.Get<float>() );
				break;
			case eFbxDouble:
				settings->setFloatValue( QString( key ), prop.Get<double>() );
				break;
			case eFbxString:
				settings->setStringValue( QString( key ), QString( prop.Get<FbxString>() ) );
				break;
			}
		}
#endif

	}

	for ( int i = 0; i < node->children.count(); i++ )
	{
		fbxImportAnim( node->children[i] );
	}
}


static QColor toQColor( FbxPropertyT<FbxDouble3> v )
{
	QColor c;

	c.setRedF( v.Get()[0] );
	c.setGreenF( v.Get()[1] );
	c.setBlueF( v.Get()[2] );

	return c;
}

/**
**/
DzTexture* DzFbxImporter::toTexture( FbxProperty fbxProperty )
{
	for ( int i = 0; i < fbxProperty.GetSrcObjectCount<FbxFileTexture>(); ++i )
	{
		const FbxFileTexture* lTex = fbxProperty.GetSrcObject<FbxFileTexture>( i );
		const DzImageMgr* imgMgr = dzApp->getImageMgr();
		DzTexture* dsTexture = imgMgr->getImage( lTex->GetFileName() );
		if ( !dsTexture )
		{
			dsTexture = imgMgr->getImage( m_folder.filePath( lTex->GetFileName() ) );
		}

		return dsTexture;
	}

	return 0;
}

/**
**/
void DzFbxImporter::fbxImportMesh( Node* node, FbxNode* fbxNode, DzNode* dsMeshNode )
{
	FbxMesh* fbxMesh = fbxNode->GetMesh();
	DzObject* dsObject = new DzObject();
	DzFacetMesh* dsMesh = new DzFacetMesh();
	DzFacetShape* dsShape = new DzFacetShape();
	DzFigure* dsFigure = qobject_cast<DzFigure*>( dsMeshNode );

	DzVec3 offset( 0, 0, 0 );
	if ( dsFigure )
	{
		offset = dsFigure->getOrigin();
	}

	// begin the edit
	dsMesh->beginEdit();

	// copy vertices
	int numVertices = fbxMesh->GetControlPointsCount();
	FbxVector4* fbxVertices = fbxMesh->GetControlPoints();
	DzPnt3* dsVertices = dsMesh->setVertexArray( numVertices );
	for ( int i = 0; i < numVertices ; i++ )
	{
		dsVertices[i][0] = fbxVertices[i][0] + offset[0];
		dsVertices[i][1] = fbxVertices[i][1] + offset[1];
		dsVertices[i][2] = fbxVertices[i][2] + offset[2];
	}

	// copy UVs
	for ( int i = 0; i < fbxMesh->GetElementUVCount(); i++ )
	{
		FbxGeometryElementUV* uvElement = fbxMesh->GetElementUV( i );
		const int numUvs = uvElement->GetDirectArray().GetCount();

		DzMap* uvMap = dsMesh->getUVs();
		uvMap->setNumValues( numUvs );
		DzPnt2* dsUVs = uvMap->getPnt2ArrayPtr();

		for ( int j = 0; j < numUvs; j++ )
		{
			FbxVector2 fbxUv = uvElement->GetDirectArray().GetAt( j );
			dsUVs[j][0] = fbxUv[0];
			dsUVs[j][1] = fbxUv[1];
		}

		// only do the first
		break;
	}

	bool needsSubd = false;

	// copy subd edge weights
	for ( int i = 0; i < fbxMesh->GetElementVertexCreaseCount(); i++ )
	{
		FbxGeometryElementCrease* element = fbxMesh->GetElementVertexCrease( i );
		const int num = element->GetDirectArray().GetCount();

		for ( int j = 0; j < num; j++ )
		{
			double v = element->GetDirectArray().GetAt( j );
			if ( v > 0 )
			{
				needsSubd = true;

#if DZ_SDK_4_12_OR_GREATER
				dsMesh->setVertexWeight( j, v );
#else
				// DzFacetMesh::setVertexWeight() is not in the 4.5 SDK, so we
				// attempt to use the meta-object to call the method.

				QMetaObject::invokeMethod( dsMesh,
					"setVertexWeight", Q_ARG( int, j ), Q_ARG( int, v ) );
#endif
			}
		}

		// only do the first
		break;
	}

	// copy materials
	for ( int i = 0; i < fbxNode->GetMaterialCount(); i++ )
	{
		QColor diffuseColor = Qt::white;
		DzTexturePtr diffuseMap = NULL;

		float diffuseFactor = 1.0f;

		float opacityBase = 1.0f;
		DzTexturePtr opacityMap = NULL;

		QColor ambientColor = Qt::black;
		DzTexturePtr ambientMap = NULL;

		float ambientFactor = 1.0f;

		QColor specularColor = Qt::white;
		DzTexturePtr specularMap = NULL;

		float specularFactor = 1.0f;

		float shininess = 1.0f;
		DzTexturePtr shininessMap = NULL;

		float reflectionFactor = 1.0f;
		DzTexturePtr reflectionMap = NULL;

		float roughness = 0.1f;
		float metallicity = 1.0f;

		FbxSurfaceMaterial* fbxMaterial = fbxNode->GetMaterial( i );
		DzMaterialPtr dsMaterial = NULL;

		const DzClassFactory* pbrMatFactory = dzApp->findClassFactory( "DzPbrMaterial" ); //or "DzUberIrayMaterial"
		if ( QObject* pbrMatInstance = pbrMatFactory ? pbrMatFactory->createInstance() : NULL )
		{
			dsMaterial = qobject_cast<DzMaterial*>( pbrMatInstance );
		}
		else
		{
			dsMaterial = new DzDefaultMaterial();
		}

		DzDefaultMaterial* dsDefMaterial = qobject_cast<DzDefaultMaterial*>( dsMaterial );

		const bool isPhong = fbxMaterial->GetClassId().Is( FbxSurfacePhong::ClassId );
		if ( isPhong )
		{
			FbxSurfacePhong* fbxPhong = static_cast<FbxSurfacePhong*>( fbxMaterial );

			diffuseColor = toQColor( fbxPhong->Diffuse );
			diffuseMap = toTexture( fbxPhong->Diffuse );

			// Maya and Max want transparency in the color
			opacityBase = 1 - (fbxPhong->TransparentColor.Get()[0] + fbxPhong->TransparentColor.Get()[1] + fbxPhong->TransparentColor.Get()[2]) / 3;
			opacityMap = toTexture( fbxPhong->TransparentColor );

			if ( dsDefMaterial )
			{
				diffuseFactor = fbxPhong->DiffuseFactor.Get();

				ambientColor = toQColor( fbxPhong->Ambient );
				ambientMap = toTexture( fbxPhong->Ambient );

				ambientFactor = fbxPhong->AmbientFactor.Get();

				specularColor = toQColor( fbxPhong->Specular );
				specularMap = toTexture( fbxPhong->Specular );

				specularFactor = fbxPhong->SpecularFactor.Get();

				shininess = fbxPhong->Shininess.Get();
				shininessMap = toTexture( fbxPhong->Shininess );

				reflectionFactor = fbxPhong->ReflectionFactor.Get();
				reflectionMap = toTexture( fbxPhong->ReflectionFactor );
			}
			else //DzPbrMaterial or DzUberIrayMaterial
			{
				roughness = 1.0 - ((log( fbxPhong->Shininess.Get() ) / log( 2.0 )) - 2) / 10;

				FbxDouble3 spec = fbxPhong->Specular.Get();
				float innerDistance = qAbs( spec[1] - spec[0] ) + qAbs( spec[2] - spec[0] ) + qAbs( spec[2] - spec[1] );
				metallicity = qMin( 1.0f, innerDistance );
			}
		}
		else if ( fbxMaterial->GetClassId().Is( FbxSurfaceLambert::ClassId ) )
		{
			FbxSurfaceLambert* fbxLambert = static_cast<FbxSurfaceLambert*>( fbxMaterial );

			diffuseColor = toQColor( fbxLambert->Diffuse );
			diffuseMap = toTexture( fbxLambert->Diffuse );

			// Maya and Max want transparency in the color
			opacityBase = 1 - (fbxLambert->TransparentColor.Get()[0] + fbxLambert->TransparentColor.Get()[1] + fbxLambert->TransparentColor.Get()[2]) / 3;
			opacityMap = toTexture( fbxLambert->TransparentColor );

			if ( dsDefMaterial )
			{
				ambientColor = toQColor( fbxLambert->Ambient );
				ambientMap = toTexture( fbxLambert->Ambient );

				ambientFactor = fbxLambert->AmbientFactor.Get();
			}
		}

		dsMaterial->setName( fbxMaterial->GetName() );

		dsMaterial->setDiffuseColor( diffuseColor );
		dsMaterial->setColorMap( diffuseMap );

		dsMaterial->setBaseOpacity( opacityBase );
		dsMaterial->setOpacityMap( opacityMap );

		if ( dsDefMaterial )
		{
			dsDefMaterial->setAmbientColor( ambientColor );
			dsDefMaterial->setAmbientColorMap( ambientMap );

			dsDefMaterial->setAmbientStrength( ambientFactor );

			if ( isPhong )
			{
				dsDefMaterial->setDiffuseStrength( diffuseFactor );

				dsDefMaterial->setSpecularColor( specularColor );
				dsDefMaterial->setSpecularColorMap( specularMap );

				dsDefMaterial->setSpecularStrength( specularFactor );

				dsDefMaterial->setGlossinessStrength( shininess );
				dsDefMaterial->setGlossinessValueMap( shininessMap );

				dsDefMaterial->setReflectionStrength( reflectionFactor );
				dsDefMaterial->setReflectionMap( reflectionMap );
			}
		}
		else if ( isPhong ) //DzPbrMaterial or DzUberIrayMaterial
		{
			// Because DzPbrMaterial is not in the public SDK, we attempt to use
			// the meta-object to call the methods. If this fails, we attempt to
			// find the properties by name and if found set their respective values.

			 //use "setGlossyRoughness" double if using DzUberIrayMaterial
			if ( !QMetaObject::invokeMethod( dsMaterial,
				"setRoughness", Q_ARG( float, roughness ) ) )
			{
				if ( DzFloatProperty* fProp = qobject_cast<DzFloatProperty*>( dsMaterial->findProperty( "Glossy Roughness" ) ) )
				{
					fProp->setValue( roughness );
				}
			}

			//use "setMetallicity" double if using DzUberIrayMaterial
			if ( !QMetaObject::invokeMethod( dsMaterial,
				"setMetallicity", Q_ARG( float, metallicity ) ) )
			{
				if ( DzFloatProperty* fProp = qobject_cast<DzFloatProperty*>( dsMaterial->findProperty( "Metallic Weight" ) ) )
				{
					fProp->setValue( metallicity );
				}
			}
		}

		m_dsMaterials.push_back( dsMaterial );

		dsShape->addMaterial( dsMaterial );
		dsMesh->activateMaterial( dsMaterial->getName() );
	}

	//check whether the material maps with only one mesh
	bool matsAllSame = true;
	for ( int i = 0; i < fbxMesh->GetElementMaterialCount(); i++ )
	{
		FbxGeometryElementMaterial* lMaterialElement = fbxMesh->GetElementMaterial( i );
		if ( lMaterialElement->GetMappingMode() == FbxGeometryElement::eByPolygon )
		{
			matsAllSame = false;
			break;
		}
	}

	if ( matsAllSame )
	{
		for ( int i = 0; i < fbxMesh->GetElementMaterialCount(); i++ )
		{
			FbxGeometryElementMaterial* lMaterialElement = fbxMesh->GetElementMaterial( i );
			if ( lMaterialElement->GetMappingMode() == FbxGeometryElement::eAllSame )
			{
				FbxSurfaceMaterial* lMaterial = fbxMesh->GetNode()->GetMaterial( lMaterialElement->GetIndexArray().GetAt( 0 ) );
				Q_UNUSED( lMaterial  );

				const int lMatId = lMaterialElement->GetIndexArray().GetAt( 0 );
				if ( lMatId >= 0 )
				{
					dsMesh->activateMaterial( lMatId );
					break;
				}
			}
		}
	}

	FbxGeometryElementPolygonGroup* polygonGroup = fbxMesh->GetElementPolygonGroup( 0 );

	QMap< QPair<int, int>, int > edgeMap;
	int numEdges = 0;

	const int numPolygons = fbxMesh->GetPolygonCount();
	int curGroupId = -1;
	for ( int i = 0; i < numPolygons; i++ )
	{
		// material
		if ( !matsAllSame )
		{
			for ( int j = 0; j < fbxMesh->GetElementMaterialCount(); j++ )
			{
				FbxGeometryElementMaterial* lMaterialElement = fbxMesh->GetElementMaterial( j );
				FbxSurfaceMaterial* lMaterial = fbxMesh->GetNode()->GetMaterial( lMaterialElement->GetIndexArray().GetAt( i ) );
				const int lMatId = lMaterialElement->GetIndexArray().GetAt( i );

				if ( lMatId >= 0 )
				{
					dsMesh->activateMaterial( lMatId );
					break;
				}
			}
		}

		if ( polygonGroup )
		{
			const int groupId = polygonGroup->GetIndexArray().GetAt( i );
			if ( groupId != curGroupId )
			{
				curGroupId = groupId;
				dsMesh->activateFaceGroup( "fbx_group_" % QString::number( groupId ) );
			}
		}


		// verts
		const int numIndices = fbxMesh->GetPolygonSize( i );
		DzFacet face;
		int triFanRoot = -1;
		for ( int j = 0; j < numIndices; j++ )
		{
			if ( numIndices <= 4 )
			{
				face.m_vertIdx[j] = fbxMesh->GetPolygonVertex( i, j );
				face.m_normIdx[j] = face.m_vertIdx[j];

				for ( int k = 0; k < fbxMesh->GetElementUVCount(); k++ )
				{
					FbxGeometryElementUV* leUV = fbxMesh->GetElementUV( k );
					switch ( leUV->GetMappingMode() )
					{
					case FbxGeometryElement::eByControlPoint:
						switch ( leUV->GetReferenceMode() )
						{
						case FbxGeometryElement::eDirect:
							face.m_uvwIdx[j] = face.m_vertIdx[j];
							break;
						case FbxGeometryElement::eIndexToDirect:
							face.m_uvwIdx[j] = leUV->GetIndexArray().GetAt( face.m_vertIdx[j] );
							break;
						default:
							break;
						}
						break;
					case FbxGeometryElement::eByPolygonVertex:
						face.m_uvwIdx[j] = fbxMesh->GetTextureUVIndex( i, j );
						break;
					default:
						break;
					}

					// only do the first UV set
					break;
				}

				if ( j == numIndices - 1 )
				{
					dsMesh->addFacet( face.m_vertIdx, face.m_uvwIdx );
				}
			}
			else if ( j >= 2 )
			{
				const bool isRoot = j == 2;

				face.m_vertIdx[0] = fbxMesh->GetPolygonVertex( i, 0 );
				face.m_vertIdx[1] = fbxMesh->GetPolygonVertex( i, j - 1 );
				face.m_vertIdx[2] = fbxMesh->GetPolygonVertex( i, j );
				face.m_vertIdx[3] = -1;
				face.m_normIdx[0] = face.m_vertIdx[0];
				face.m_normIdx[1] = face.m_vertIdx[1];
				face.m_normIdx[2] = face.m_vertIdx[2];
				face.m_normIdx[3] = face.m_vertIdx[3];

				if ( isRoot )
				{
					triFanRoot = dsMesh->getNumFacets();
				}

#if DZ_SDK_4_12_OR_GREATER
				face.setTriFanRoot( triFanRoot );
#else
				// DzFacet::setTriFanRoot() is not in the 4.5 SDK, and DzFacet
				// is not derived from QObject, so we must modify the member
				// directly.

				face.m_vertIdx[3] = -(triFanRoot + 2);
#endif

				if ( isRoot )
				{
#if DZ_SDK_4_12_OR_GREATER
					face.setTriFanCount( numIndices - 2 );
#else
					// DzFacet::setTriFanCount() is not in the 4.5 SDK, and DzFacet
					// is not derived from QObject, so we must modify the member
					// directly.

					face.m_edges[3] = -numIndices;
#endif
				}
				else
				{
#if DZ_SDK_4_12_OR_GREATER
					face.clearTriFanCount();
#else
					// DzFacet::clearTriFanCount() is not in the 4.5 SDK, and DzFacet
					// is not derived from QObject, so we must modify the member
					// directly.

					face.m_edges[3] = -1;
#endif
				}


				for ( int k = 0; k < fbxMesh->GetElementUVCount(); k++ )
				{
					FbxGeometryElementUV* leUV = fbxMesh->GetElementUV( k );
					switch ( leUV->GetMappingMode() )
					{
					case FbxGeometryElement::eByControlPoint:
						switch ( leUV->GetReferenceMode() )
						{
						case FbxGeometryElement::eDirect:
							face.m_uvwIdx[0] = face.m_vertIdx[0];
							face.m_uvwIdx[1] = face.m_vertIdx[j - 1];
							face.m_uvwIdx[2] = face.m_vertIdx[j];
							face.m_uvwIdx[3] = -1;
							break;
						case FbxGeometryElement::eIndexToDirect:
							face.m_uvwIdx[0] = leUV->GetIndexArray().GetAt( face.m_vertIdx[0] );
							face.m_uvwIdx[1] = leUV->GetIndexArray().GetAt( face.m_vertIdx[j - 1] );
							face.m_uvwIdx[2] = leUV->GetIndexArray().GetAt( face.m_vertIdx[j] );
							face.m_uvwIdx[3] = -1;
							break;
						default:
							break;
						}
						break;
					case FbxGeometryElement::eByPolygonVertex:
						face.m_uvwIdx[0] = fbxMesh->GetTextureUVIndex( i, j );
						face.m_uvwIdx[1] = fbxMesh->GetTextureUVIndex( i, j - 1 );
						face.m_uvwIdx[2] = fbxMesh->GetTextureUVIndex( i, j );
						face.m_uvwIdx[3] = -1;
						break;
					default:
						break;
					}

					// only do the first UV set
					break;
				}

#if DZ_SDK_4_12_OR_GREATER
				dsMesh->addFacet( face );
#else
				// DzFacetMesh::addFacet() is not in the 4.5 SDK, so we attempt
				// to use the meta-object to call the method.

				QMetaObject::invokeMethod( dsMesh,
					"addFacet", Q_ARG( const DzFacet &, face ) );
#endif

				if ( isRoot )
				{
#if DZ_SDK_4_12_OR_GREATER
					dsMesh->incrementNgons();
#else
					// DzFacetMesh::incrementNgons() is not in the 4.5 SDK, so
					// we attempt to use the meta-object to call the method.

					QMetaObject::invokeMethod( dsMesh, "incrementNgons" );
#endif
				}
			}


			{
				int j0 = j;
				int j1 = ( j + 1 ) % numIndices;

				int a = fbxMesh->GetPolygonVertex( i, j0 );
				int b = fbxMesh->GetPolygonVertex( i, j1 );
				QPair<int, int> pair( qMin( a, b ), qMax( a, b ) );
				if ( !edgeMap.contains( pair ) )
				{
					edgeMap[pair] = numEdges;
					numEdges++;
				}
			}
		}
	}

	for ( int i = 0; i < fbxMesh->GetElementEdgeCreaseCount(); i++ )
	{
		FbxGeometryElementCrease* element = fbxMesh->GetElementEdgeCrease( i );
		int num = element->GetDirectArray().GetCount();
		Q_UNUSED( num )

		QMap< QPair<int, int>, int >::iterator j;
		for ( j = edgeMap.begin(); j != edgeMap.end(); ++j )
		{
			int jj = j.value();
			float v = element->GetDirectArray().GetAt( jj );
			int v0 = j.key().first;
			int v1 = j.key().second;
			if ( v > 0 )
			{
				needsSubd = true;
#if DZ_SDK_4_12_OR_GREATER
				dsMesh->setEdgeWeight( v0, v1, v );
#else
				// DzFacetMesh::setEdgeWeight() is not in the 4.5 SDK, so we
				// attempt to use the meta-object to call the method.

				QMetaObject::invokeMethod( dsMesh,
					"setEdgeWeight", Q_ARG( int, v0 ), Q_ARG( int, v1 ), Q_ARG( int, v ) );
#endif
			}
		}

		// only do the first
		break;
	}


	// end the edits
	dsMesh->finishEdit();
	dsShape->setFacetMesh( dsMesh );

	if ( needsSubd )
	{
		dsMesh->enableSubDivision( true );

		if ( DzEnumProperty* lodControl = dsShape->getLODControl() )
		{
			lodControl->setValue( lodControl->getNumItems() - 1 ); //set to high res
			lodControl->setDefaultValue( lodControl->getNumItems() - 1 ); //set to high res
		}
	}

	dsObject->addShape( dsShape );
	dsMeshNode->setObject( dsObject );


	for ( int i = 0; i < fbxMesh->GetDeformerCount(); i++ )
	{
		FbxDeformer* fbxDeformer = fbxMesh->GetDeformer( i );
		if ( dsFigure && fbxDeformer->GetClassId().Is( FbxSkin::ClassId ) )
		{
			Skinning skinning;
			skinning.node = node;
			skinning.fbxSkin = static_cast< FbxSkin* >( fbxDeformer );
			skinning.dsFigure = dsFigure;
			skinning.numVertices = numVertices;

			m_skins.push_back( skinning );
		}
		else if ( fbxDeformer->GetClassId().Is( FbxBlendShape::ClassId ) )
		{
			DzPnt3* values = new DzPnt3[numVertices];

			FbxBlendShape* fbxBlendShape = static_cast< FbxBlendShape* >( fbxDeformer );

			DzProgress progress( "Morphs", fbxBlendShape->GetBlendShapeChannelCount() );
			for ( int j = 0; j < fbxBlendShape->GetBlendShapeChannelCount(); j++ )
			{
				FbxBlendShapeChannel* fbxBlendChannel = fbxBlendShape->GetBlendShapeChannel( j );
				DzMorph* dsMorph = new DzMorph;
				dsMorph->setName( fbxBlendChannel->GetName() );
				DzMorphDeltas* dsDeltas = dsMorph->getDeltas();

				QString name = dsMorph->getName();

				DzFloatProperty* morphControl = NULL;
#if DZ_SDK_4_12_OR_GREATER
				morphControl = dsMorph->getValueControl();
#else
				// DzMorph::getValueControl() is not in the 4.5 SDK, but DzMorph::getValueChannel()
				// was, so we use the previous name.

				morphControl = dsMorph->getValueChannel();
#endif

				applyFbxCurve( fbxBlendChannel->DeformPercent.GetCurve( m_fbxAnimLayer ), morphControl, 0.01 );

				for ( int v = 0; v < numVertices; v++ )
				{
					values[v][0] = 0;
					values[v][1] = 0;
					values[v][2] = 0;
				}

				for ( int k = 0; k < fbxBlendChannel->GetTargetShapeCount(); k++ )
				{
					FbxShape* fbxShape = fbxBlendChannel->GetTargetShape( k );
					FbxVector4* fbxShapeVerts = fbxShape->GetControlPoints();
					int* fbxShapeIndices = fbxShape->GetControlPointIndices();
					//double weight = fbxBlendChannel->GetTargetShapeFullWeights()[k];

					if ( fbxShapeIndices )
					{
						for ( int vv = 0; vv < fbxShape->GetControlPointIndicesCount(); vv++ )
						{
							int v = fbxShapeIndices[vv];
							values[v][0] = fbxShapeVerts[v][0] - fbxVertices[v][0];
							values[v][1] = fbxShapeVerts[v][1] - fbxVertices[v][1];
							values[v][2] = fbxShapeVerts[v][2] - fbxVertices[v][2];
						}
					}
					else
					{
						for ( int v = 0; v < fbxShape->GetControlPointsCount(); v++ )
						{
							values[v][0] = fbxShapeVerts[v][0] - fbxVertices[v][0];
							values[v][1] = fbxShapeVerts[v][1] - fbxVertices[v][1];
							values[v][2] = fbxShapeVerts[v][2] - fbxVertices[v][2];
						}
					}
				}

				DzIntArray indexes;
				DzTArray<DzVec3> deltas;
				for ( int v = 0; v < numVertices; v++ )
				{
					if ( values[v][0] != 0 || values[v][1] != 0 || values[v][2] != 0 )
					{
						indexes.append( v );
						deltas.append( DzVec3( values[v][0], values[v][1], values[v][2] ) );
					}
				}
				dsDeltas->addDeltas( indexes, deltas, false );
				dsObject->addModifier( dsMorph );

				progress.step();
			}

			delete[] values;
		}
	}
}

void DzFbxImporter::applyFbxCurve( FbxAnimCurve* fbxCurve, DzFloatProperty* dsProperty, double scale )
{
	if ( !fbxCurve || !dsProperty )
	{
		return;
	}

	dsProperty->deleteAllKeys();

	for ( int i = 0; i < fbxCurve->KeyGetCount(); i++ )
	{
		const double fbxTime = fbxCurve->KeyGetTime( i ).GetSecondDouble();
		const double fbxValue = fbxCurve->KeyGetValue( i );
		DzTime dsTime = static_cast< DzTime >((fbxTime * DZ_TICKS_PER_SECOND) + 0.5f); //round to nearest tick
		m_dsEndTime = qMax( m_dsEndTime, dsTime );

		dsProperty->setValue( dsTime, fbxValue * scale );
	}
}

///////////////////////////////////////////////////////////////////////
// DzFbxImportFrame
///////////////////////////////////////////////////////////////////////

struct DzFbxImportFrame::Data
{
	Data( DzFbxImporter* importer ) :
		m_importer( importer ),
		m_animTakeCmb( NULL )
	{}

	DzFbxImporter*	m_importer;
	QComboBox*		m_animTakeCmb;
};

/**
**/
DzFbxImportFrame::DzFbxImportFrame( DzFbxImporter* importer, const QStringList &animChoices, const QString &errors ) :
	DzFileIOFrame( tr( "FBX Import Options" ) ), m_data( new Data( importer ) )
{
	const QString name = "FbxImport";

	const int margin = style()->pixelMetric( DZ_PM_GeneralMargin );
	const int btnHeight = style()->pixelMetric( DZ_PM_ButtonHeight );

	QVBoxLayout* mainLyt = new QVBoxLayout( this );
	mainLyt->setMargin( margin );
	mainLyt->setSpacing( margin );

	QBoxLayout* animLyt = new QHBoxLayout();
	animLyt->setMargin( 0 );
	animLyt->setSpacing( margin );

	QLabel* lbl = new QLabel( tr( "Take to Import:" ) );
	lbl->setObjectName( name % "TakeToImportLbl" );
	animLyt->addWidget( lbl );

	m_data->m_animTakeCmb = new QComboBox();
	m_data->m_animTakeCmb->setObjectName( name % "TakeToImportCmb" );
	m_data->m_animTakeCmb->addItems( animChoices );
	m_data->m_animTakeCmb->setFixedHeight( btnHeight );
	animLyt->addWidget( m_data->m_animTakeCmb );

	mainLyt->addLayout( animLyt );

	QLabel* errorLbl = new QLabel();
	if ( errors.size() )
	{
		errorLbl->setText( tr( "Errors:\n" ) % errors );
	}
	mainLyt->addSpacing( 4 );
	mainLyt->addWidget( errorLbl );

	DzFbxImportFrame::resetOptions();
}

/**
**/
DzFbxImportFrame::~DzFbxImportFrame()
{}

/**
**/
void DzFbxImportFrame::setOptions( const DzFileIOSettings* options, const QString &filename )
{
	assert( options );
	if ( !options )
	{
		return;
	}

	const QString take = options->getStringValue( c_optionTake );
	for ( int i = 0; i < m_data->m_animTakeCmb->count(); i++ )
	{
		if ( m_data->m_animTakeCmb->itemText( i ) == take )
		{
			m_data->m_animTakeCmb->setCurrentIndex( i );
			break;
		}
	}
}

/**
**/
void DzFbxImportFrame::getOptions( DzFileIOSettings* options ) const
{
	assert( options );
	if ( !options )
	{
		return;
	}

	options->setStringValue( c_optionTake, m_data->m_animTakeCmb->currentText() );
}

/**
**/
void DzFbxImportFrame::applyChanges()
{
}

/**
**/
void DzFbxImportFrame::resetOptions()
{
}

#include "moc_dzfbximporter.cpp"