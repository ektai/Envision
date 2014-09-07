/***********************************************************************************************************************
**
** Copyright (c) 2011, 2014 ETH Zurich
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
** following conditions are met:
**
** * Redistributions of source code must retain the above copyright notice, this list of conditions and the following
** disclaimer.
** * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
** following disclaimer in the documentation and/or other materials provided with the distribution.
** * Neither the name of the ETH Zurich nor the names of its contributors may be used to endorse or promote products
** derived from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
** INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
** WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
***********************************************************************************************************************/

#include "Diff.h"

#include "GitRepository.h"

#include "../simple/Parser.h"

namespace FilePersistence {

Diff::Diff(){}

Diff::Diff(QList<GenericNode*>& oldNodes, std::shared_ptr<GenericTree> oldTree,
			  QList<GenericNode*>& newNodes, std::shared_ptr<GenericTree> newTree,
			  const GitRepository* repository)
{
	oldTree_ = oldTree;
	newTree_ = newTree;

	IdToGenericNodeHash oldNodesHash;
	for (auto node : oldNodes)
		oldNodesHash.insertMulti(node->id(), node);

	IdToGenericNodeHash newNodesHash;
	for (auto node : newNodes)
		newNodesHash.insertMulti(node->id(), node);

	IdToGenericNodeHash newlyCreatedParents;
	findParentsInCommit(oldNodesHash, newlyCreatedParents, oldTree_, repository);
	findParentsInCommit(newNodesHash, newlyCreatedParents, newTree_, repository);

	filterPersistenceUnits(oldNodesHash);
	filterPersistenceUnits(newNodesHash);

	idMatching(oldNodesHash, newNodesHash, newlyCreatedParents);

	markChildUpdates();
}

IdToChangeDescriptionHash Diff::changes(ChangeType type) const
{
	IdToChangeDescriptionHash changesOfType;
	for (auto change : changeDescriptions_.values())
	{
		if (change->type() == type)
			changesOfType.insert(change->id(), change);
	}
	return changesOfType;
}

IdToChangeDescriptionHash Diff::changes(ChangeType type, ChangeDescription::UpdateFlags flags) const
{
	IdToChangeDescriptionHash changesOfType;
	for (auto change : changeDescriptions_.values())
	{
		if (change->type() == type && change->hasAtLeastFlags(flags))
			changesOfType.insert(change->id(), change);
	}
	return changesOfType;
}

// Private methods
void Diff::idMatching(IdToGenericNodeHash& oldNodes, IdToGenericNodeHash& newNodes,
							 IdToGenericNodeHash& createdParents)
{
	QSet<Model::NodeIdType> onlyInNewNodes = QSet<Model::NodeIdType>::fromList(newNodes.keys());
	QList<GenericNode*> oldNodesValueList = oldNodes.values();

	IdToGenericNodeHash::iterator iter;
	for (auto oldNode : oldNodesValueList)
	{
		iter = newNodes.find(oldNode->id());
		if (iter == newNodes.end())
		{
			// no such id in newNodes
			changeDescriptions_.insert(oldNode->id(), new ChangeDescription(oldNode, nullptr));
		}
		else
		{
			// found id
			changeDescriptions_.insert(oldNode->id(), new ChangeDescription(oldNode, iter.value()));
			// id is also present in oldNodes
			onlyInNewNodes.remove(iter.key());
		}
	}

	for (auto newId : onlyInNewNodes)
	{
		iter = newNodes.find(newId);
		changeDescriptions_.insert(newId, new ChangeDescription(nullptr, iter.value()));
	}

	for (auto parent : createdParents.values())
		changeDescriptions_.insert(parent->id(), new ChangeDescription(parent, parent));
}

void Diff::findParentsInCommit(IdToGenericNodeHash& nodes, IdToGenericNodeHash& createdParents,
										 std::shared_ptr<GenericTree> tree, const GitRepository* repository)
{
	QHash<QString, Model::NodeIdType> fileToNodeIDs;
	for (auto node : nodes.values())
		fileToNodeIDs.insertMulti(node->persistentUnit()->name(), node->id());

	const QString fullFile("Diff::findParentsInCommit");

	IdToGenericNodeHash::iterator iter = nodes.end();
	for (auto path : fileToNodeIDs.uniqueKeys())
	{
		GenericPersistentUnit* unit = tree->persistentUnit(path);
		const CommitFile* file = repository->getCommitFile(tree->commitName(), path);
		GenericNode* root = Parser::load(file->content_, file->size_, false, tree->newPersistentUnit(fullFile));
		for (auto id : fileToNodeIDs.values(path))
		{
			GenericNode* nodeInFile = root->find(id);
			GenericNode* parentInFile = nodeInFile->parent();
			Q_ASSERT(nodes.contains(id));
			iter = nodes.find(id);
			GenericNode* node = iter.value();
			iter = nodes.find(parentInFile->id());
			GenericNode* parent = nullptr;
			if (iter == nodes.end())
			{
				parent = unit->newNode(parentInFile);
				createdParents.insert(parent->id(), parent);
			}
			else
				parent = iter.value();
			node->setParent(parent);
			parent->addChild(node);
		}
		tree->remove(fullFile);
	}
}

void Diff::markChildUpdates()
{
	for (ChangeDescription* change : changeDescriptions_.values())
	{
		switch (change->type())
		{
			case ChangeType::Added:
			{
				GenericNode* parent = change->newNode()->parent();
				if (parent)
				{
					ChangeDescription* parentChange = changeDescriptions_.value(parent->id());
					Q_ASSERT(parentChange);
					parentChange->setChildrenUpdate(true);
				}
				break;
			}

			case ChangeType::Deleted:
			{
				GenericNode* parent = change->oldNode()->parent();
				if (parent)
				{
					ChangeDescription* parentChange = changeDescriptions_.value(parent->id());
					Q_ASSERT(parentChange);
					parentChange->setChildrenUpdate(true);
				}
				break;
			}

			case ChangeType::Moved:
			{
				GenericNode* parent = change->oldNode()->parent();
				if (parent)
				{
					ChangeDescription* parentChange = changeDescriptions_.value(parent->id());
					Q_ASSERT(parentChange);
					parentChange->setChildrenUpdate(true);
				}

				parent = change->newNode()->parent();
				if (parent)
				{
					ChangeDescription* parentChange = changeDescriptions_.value(parent->id());
					Q_ASSERT(parentChange);
					parentChange->setChildrenUpdate(true);
				}

				break;
			}

			case ChangeType::Stationary:
			{
				ChangeDescription::UpdateFlags flags = change->flags();
				if (flags.testFlag(ChangeDescription::Order))
				{
					GenericNode* parent = change->oldNode()->parent();
					if (parent)
					{
						ChangeDescription* parentChange = changeDescriptions_.value(parent->id());
						Q_ASSERT(parentChange);
						parentChange->setChildrenUpdate(true);
					}
				}
				break;
			}

			default:
				Q_ASSERT(false);
		}
	}
}

void Diff::filterPersistenceUnits(IdToGenericNodeHash& nodes)
{
	for (auto key : nodes.uniqueKeys())
	{
		if (nodes.count(key) == 2)
		{
			GenericNode* persistenceUnitDefinition = nullptr;
			GenericNode* persistenceUnitRoot = nullptr;

			QList<GenericNode*> persistenceList = nodes.values(key);
			if (GenericNode::persistentUnitType.compare(persistenceList.first()->type()) == 0)
			{
				persistenceUnitDefinition = persistenceList.first();
				persistenceUnitRoot = persistenceList.last();
			}
			else
			{
				persistenceUnitDefinition = persistenceList.last();
				persistenceUnitRoot = persistenceList.first();
			}

			persistenceUnitRoot->setParent(persistenceUnitDefinition->parent());

			nodes.remove(key);
			nodes.insert(key, persistenceUnitRoot);
		}

		Q_ASSERT(nodes.count(key) == 1);
	}
}

} /* namespace FilePersistence */
