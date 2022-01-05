/**********************************************************************
	Copyright (C) 2012-2021 Daz 3D, Inc. All Rights Reserved.

	This is UNPUBLISHED PROPRIETARY SOURCE CODE of Daz 3D, Inc;
	the contents of this file may not be disclosed to third parties,
	copied or duplicated in any form, in whole or in part, without the
	prior written permission of Daz 3D, Inc.
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

#include <fbxsdk.h>

/****************************
	Forward declarations
****************************/

class QComboBox;

class DzFigure;
class DzFloatProperty;
class DzMaterial;
class DzNode;
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
	};

	void		fbxPreRecurse( FbxNode* fbxNode );

	void		handleFbxMesh( Node* node, FbxNode* fbxNode, DzNode* dsParent );
	void		handleFbxCurve( FbxAnimCurve* fbxCurve, DzFloatProperty* dsProperty, double scale = 1 );
	DzTexture*	toTexture( FbxProperty fbxProperty );

	void		fbxImportGraph( Node* node );
	void		fbxImportSkin( Node* node );
	void		fbxImportAnim( Node* node );

	void		replicateSkeleton( DzSkeleton* crossSkeleton, Skinning &skinning );

	FbxManager*				m_fbxManager;
	FbxScene*				m_fbxScene;
	FbxAnimStack*			m_fbxAnimStack;
	FbxAnimLayer*			m_fbxAnimLayer;
	QVector<Skinning>		m_skins;
	QMap<FbxNode*, DzNode*>	m_nodeMap;
	bool					m_needConversion;
	DzTime					m_dsEndTime;
	bool					m_rigErrorPre;
	bool					m_rigErrorSkin;
	bool					m_rigErrorScale;
	bool					m_rigErrorRoot;
	QDir					m_folder;
	QVector<DzMaterial*>	m_dsMaterials;

	bool					m_suppressRigErrors;

	Node*		m_root;
};


class DzFbxImportFrame : public DzFileIOFrame {
	Q_OBJECT
public:
	DzFbxImportFrame( DzFbxImporter* importer, const QStringList &animChoices, const QString &errors );
	~DzFbxImportFrame();

	//
	// RE-IMPLEMENTATIONS
	//

	////////////////////
	//from DzFileIOFrame
	virtual void	setOptions( const DzFileIOSettings* options, const QString &filename );
	virtual void	getOptions( DzFileIOSettings* options ) const;

protected:

	////////////////////
	//from DzOptionsFrame
	virtual void	applyChanges();
	virtual void	resetOptions();

private:

	struct Data;
	QScopedPointer<Data> m_data;
};
