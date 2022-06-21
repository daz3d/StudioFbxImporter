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

	void		fbxImportMesh( Node* node, FbxNode* fbxNode, DzNode* dsMeshNode );
	void		applyFbxCurve( FbxAnimCurve* fbxCurve, DzFloatProperty* dsProperty, double scale = 1 );
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
