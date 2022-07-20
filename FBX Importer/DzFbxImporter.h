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

#pragma once

/****************************
	Include files
****************************/

#include <QtCore/QObject>
#include <QtCore/QDir>
#include <QtCore/QMap>

#include "dzfileio.h"
#include "dzimporter.h"
#include "dzvec3.h"
#include "dzweightmap.h"

#include <fbxsdk.h>

/****************************
	Forward declarations
****************************/

class QComboBox;

class DzFacetMesh;
class DzFacetShape;
class DzFigure;
class DzFloatProperty;
class DzMaterial;
class DzNode;
class DzObject;
class DzSkeleton;
class DzTexture;

/****************************
	Class definitions
****************************/

class DzFbxImporter : public DzImporter {
	Q_OBJECT
public:
	DzFbxImporter();
	virtual ~DzFbxImporter();

	//
	// RE-IMPLEMENTATIONS
	//

	////////////////////
	//from DzFileIO
	virtual void		getDefaultOptions( DzFileIOSettings* options ) const;

	////////////////////
	//from DzImporter
	virtual bool		recognize( const QString &filename ) const;
	virtual int			getNumExtensions() const;
	virtual QString		getExtension( int i ) const;
	virtual QString		getDescription() const;

	virtual DzError		read( const QString &filename, const DzFileIOSettings* options );

	//
	// IMPLEMENTATIONS
	//

	QString		getFileVersion() const;
	QString		getFileCreator() const;
	QString		getFileFormat() const;

	QString		getSceneAuthor() const;
	QString		getSceneTitle() const;
	QString		getSceneSubject() const;
	QString		getSceneKeywords() const;
	QString		getSceneRevision() const;
	QString		getSceneComment() const;
	QString		getOriginalAppVendor() const;
	QString		getOriginalAppName() const;
	QString		getOriginalAppVersion() const;

	QStringList	getAnimStackNames() const;
	QStringList	getErrorList() const;

public slots:

	void		setRotationLimits( bool enable );
	void		setIncludeAnimations( bool enable );
	void		setTakeName( const QString &name );

	void		setIncludePolygonSets( bool enable );
	void		setIncludePolygonGroups( bool enable );

	void		setStudioNodeNamesLabels( bool enable );
	void		setStudioNodePresentation( bool enable );
	void		setStudioNodeSelectionMap( bool enable );
	void		setStudioSceneIDs( bool enable );

protected:

	int		getOptions( DzFileIOSettings* options, const DzFileIOSettings* impOptions, const QString &filename );

private:

	class Node {
	public:
		Node() :
			parent( 0 ),
			dsParent( 0 ),
			dsNode( 0 ),
			fbxNode( 0 ),
			bindTranslation( 0, 0, 0 ),
			collapseTranslation( false )
		{}

		void setParent( Node* parent )
		{
			this->parent = parent;
			if ( this->parent )
			{
				this->parent->children.append( this );
			}
		}

		Node* find( DzNode* inDazNode )
		{
			if ( !inDazNode )
			{
				return NULL;
			}

			if ( dsNode == inDazNode )
			{
				return this;
			}

			for ( int i = 0; i < children.count(); i++ )
			{
				Node* child = children[i]->find( inDazNode );
				if ( child )
				{
					return child;
				}
			}

			return NULL;
		}

		Node*			parent;
		QVector<Node*>	children;
		DzNode*			dsParent;
		DzNode*			dsNode;
		FbxNode*		fbxNode;
		DzVec3			bindTranslation;
		bool			collapseTranslation;
	};

	struct Skinning
	{
		Node*		node;
		FbxSkin*	fbxSkin;
		DzFigure*	dsFigure;
		int			numVertices;
		DzWeightMapPtr	m_blendWeights;
	};


	void		fbxPreImportAnimationStack();
	void		fbxPreImportGraph( FbxNode* fbxNode );
	void		fbxPreImport();

	DzTexture*	toTexture( FbxProperty fbxProperty );

	void		fbxImportVertices( int numVertices, FbxVector4* fbxVertices, DzFacetMesh* dsMesh, DzVec3 offset );
	void		fbxImportUVs( FbxMesh* fbxMesh, DzFacetMesh* dsMesh );
	void		fbxImportSubdVertexWeights( FbxMesh* fbxMesh, DzFacetMesh* dsMesh, bool &enableSubd );
	void		fbxImportMaterials( FbxNode* fbxNode, FbxMesh* fbxMesh, DzFacetMesh* dsMesh, DzFacetShape* dsShape, bool &matsAllSame );
	void		fbxImportPolygonSets( DzNode* dsMeshNode, DzFacetMesh* dsMesh, DzFacetShape* dsShape );
	void		fbxImportFaces( FbxMesh* fbxMesh, DzFacetMesh* dsMesh, bool matsAllSame, QMap<QPair<int, int>, int> &edgeMap );
	void		fbxImportSubdEdgeWeights( FbxMesh* fbxMesh, DzFacetMesh* dsMesh, QMap<QPair<int, int>, int> edgeMap, bool &enableSubd );
	void		fbxImportSkinBinding( FbxDeformer* fbxDeformer, Node* node, DzFigure* dsFigure, int numVertices );
	void		fbxImportMorph( FbxDeformer* fbxDeformer, DzObject* dsObject, int numVertices, FbxVector4* fbxVertices );
	void		fbxImportMeshModifiers( Node* node, FbxMesh* fbxMesh, DzObject* dsObject, DzFigure* dsFigure, int numVertices, FbxVector4* fbxVertices );
	void		fbxImportMesh( Node* node, FbxNode* fbxNode, DzNode* dsMeshNode );
	void		setSubdEnabled( bool onOff, DzFacetMesh* dsMesh, DzFacetShape* dsShape );

	void		applyFbxCurve( FbxAnimCurve* fbxCurve, DzFloatProperty* dsProperty, double scale = 1 );

	void		fbxImportGraph( Node* node );
	void		fbxImportAnim( Node* node );

	void		updateSelectionMap( Node* node );

	void		replicateSkeleton( DzSkeleton* crossSkeleton, Skinning &skinning );

	void		fbxRead( const QString &filename );
	void		fbxImport();
	void		fbxCleanup();


	bool				m_fbxRead;
	FbxManager*			m_fbxManager;
	FbxScene*			m_fbxScene;

	QStringList			m_animStackNames;
	FbxAnimStack*		m_fbxAnimStack;
	FbxAnimLayer*		m_fbxAnimLayer;

	int					m_fbxFileMajor;
	int					m_fbxFileMinor;
	int					m_fbxFileRevision;
	FbxString			m_fbxFileCreator;
	int					m_fbxFileBinary;

	FbxString			m_fbxSceneAuthor;
	FbxString			m_fbxSceneTitle;
	FbxString			m_fbxSceneSubject;
	FbxString			m_fbxSceneKeywords;
	FbxString			m_fbxSceneRevision;
	FbxString			m_fbxSceneComment;
	FbxString			m_fbxOrigAppVendor;
	FbxString			m_fbxOrigAppName;
	FbxString			m_fbxOrigAppVersion;

	QVector<Skinning>		m_skins;
	QMap<FbxNode*, DzNode*>	m_nodeMap;
	QMap<Node*, QString>	m_nodeFaceGroupMap;
	bool					m_needConversion;
	DzTime					m_dsEndTime;

	bool					m_suppressRigErrors;
	QStringList				m_errorList;

	QDir					m_folder;

	QVector<DzMaterial*>	m_dsMaterials;

	bool		m_includeRotationLimits;
	bool		m_includeAnimations;
	QString		m_takeName;

	bool		m_includePolygonSets;
	bool		m_includePolygonGroups;

	bool		m_studioNodeNamesLabels;
	bool		m_studioNodePresentation;
	bool		m_studioNodeSelectionMap;
	bool		m_studioSceneIDs;

	Node*		m_root;
};


class DzFbxImportFrame : public DzFileIOFrame {
	Q_OBJECT
public:
	DzFbxImportFrame( DzFbxImporter* importer );
	~DzFbxImportFrame();

	//
	// RE-IMPLEMENTATIONS
	//

	////////////////////
	//from DzFileIOFrame
	virtual void	setOptions( const DzFileIOSettings* settings, const QString &filename );
	virtual void	getOptions( DzFileIOSettings* settings ) const;

protected:

	////////////////////
	//from DzOptionsFrame
	virtual void	applyChanges();
	virtual void	resetOptions();

private:

	struct Data;
	QScopedPointer<Data> m_data;
};
