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

#include "GitRepository.h"

#include "../simple/GenericTree.h"

namespace FilePersistence {

// === Libgit2 call-back functions ===

struct GitDiffExtract
{
	GenericTree* oldTree_;
	GenericTree* newTree_;

	QList<GenericNode*> oldNodes_;
	QList<GenericNode*> newNodes_;

	bool reverseOldNew_;
};

struct GitCommitExtract
{
		git_repository* repository_;
		QList<CommitFile> files_;
};

struct GitTreeBlobs
{
	git_repository* repository_;
	QList<QString> fileNames_;
	QList<git_blob*> blobs_;

	~GitTreeBlobs()
	{
		for (auto blob : blobs_)
			git_blob_free(blob);
	}
};

int gitDiffExtractFileCallBack(
				 const git_diff_delta *delta,
				 float /*progress*/,
				 void* carryAlongData)
{
	GitDiffExtract* data = (GitDiffExtract*) carryAlongData;

	QString oldRelativePath(delta->old_file.path);
	data->oldTree_->newPersistentUnit(oldRelativePath);

	QString newRelativePath(delta->new_file.path);
	data->newTree_->newPersistentUnit(newRelativePath);

	return 0;
}

int gitDiffExtractLineCallBack(
				 const git_diff_delta* delta,
				 const git_diff_hunk* /*hunk*/,
				 const git_diff_line* line,
				 void* carryAlongData)
{
	GitDiffExtract* data = (GitDiffExtract*) carryAlongData;

	size_t lineLength = line->content_len;
	while (line->content[lineLength] == '\t')
		lineLength--;

	if ((!data->reverseOldNew_ && line->origin == GIT_DIFF_LINE_ADDITION) ||
		 (data->reverseOldNew_ && line->origin == GIT_DIFF_LINE_DELETION))
	{
		// appear on new side
		QString relativePath(delta->new_file.path);
		GenericPersistentUnit* unit = data->newTree_->persistentUnit(relativePath);
		Q_ASSERT(unit != nullptr);
		GenericNode* node = unit->newNode(line->content, lineLength);
		data->newNodes_.append(node);
	}
	else if ((!data->reverseOldNew_ && line->origin == GIT_DIFF_LINE_DELETION) ||
				(data->reverseOldNew_ && line->origin == GIT_DIFF_LINE_ADDITION))
	{
		// appear on old side
		QString relativePath(delta->old_file.path);
		GenericPersistentUnit* unit = data->oldTree_->persistentUnit(relativePath);
		Q_ASSERT(unit != nullptr);
		GenericNode* node = unit->newNode(line->content, lineLength);
		data->oldNodes_.append(node);
	}
	else
		Q_ASSERT(false);

	return 0;
}

// Tree Walk Callback
int treeWalkCommitExtractCallBack(const char* root,
									 const git_tree_entry* entry,
									 void* walkData)
{
	GitCommitExtract* data = (GitCommitExtract*) walkData;

	git_object* obj = nullptr;
	int errorCode = git_tree_entry_to_object(&obj, data->repository_, entry);

	if (errorCode == 0)
	{
		if (git_object_type(obj) == GIT_OBJ_BLOB)
		{
			QString rootPath(root);
			QString fileName(git_tree_entry_name(entry));
			QString relativePath = rootPath.append(fileName);

			git_blob* blob = (git_blob*)obj;

			qint64 contentSize = git_blob_rawsize(blob);

			const char* rawContent = (const char*)git_blob_rawcontent(blob);
			char* content = new char[contentSize];
			memcpy(content, rawContent, contentSize);

			CommitFile file(relativePath, contentSize, content);
			data->files_.append(file);

			git_blob_free(blob);
		}
	}
	else
		Q_ASSERT(false);

	return 0;
}

const QString GitRepository::WORKDIR = "GitRepositoryWorkdir";
const QString GitRepository::INDEX = "GitRepositoryIndex";

GitRepository::GitRepository(QString path)
{
	// Bugfix: initialize git threads is usually done automatically
	// but there seem to be a bug related to libgit2 and Qtf
	git_threads_init();

	path_ = path;
	repository_ = nullptr;

	if (path_.endsWith(".git"))
		path_.chop(4);

	int error = git_repository_open(&(repository_), path_.toStdString().c_str());
	checkError(error);
}

GitRepository::~GitRepository()
{
	git_repository_free(repository_);

	// Bugfix: shutdown git threads is usually done automatically
	// but there seem to be a bug related to libgit2 and Qt
	git_threads_shutdown();
}

Diff GitRepository::diff(QString oldRevision, QString newRevision) const
{
	int errorCode = 0;

	DiffKind diffKind = kind(oldRevision, newRevision);
	Q_ASSERT(diffKind != DiffKind::Unspecified);


	git_diff* gitDiff = nullptr;

	git_diff_options diffOptions;
	errorCode = git_diff_init_options(&diffOptions, GIT_DIFF_OPTIONS_VERSION);
	diffOptions.context_lines = 0;

	git_commit* oldGitCommit = nullptr;
	git_commit* newGitCommit = nullptr;
	git_tree* oldGitTree = nullptr;
	git_tree* newGitTree = nullptr;

	bool reverseOldNew = false;
	switch (diffKind)
	{
		case DiffKind::WorkdirToWorkdir:
			return Diff();
			break;

		case DiffKind::WorkdirToIndex:
			errorCode = git_diff_index_to_workdir(&gitDiff, repository_, nullptr, &diffOptions);
			reverseOldNew = true;
			break;

		case DiffKind::WorkdirToCommit:
			newGitCommit = parseCommit(newRevision);
			errorCode = git_commit_tree(&newGitTree, newGitCommit);
			checkError(errorCode);

			errorCode = git_diff_tree_to_workdir_with_index(&gitDiff, repository_, newGitTree, &diffOptions);
			reverseOldNew = true;
			break;

		case DiffKind::IndexToWorkdir:
			errorCode = git_diff_index_to_workdir(&gitDiff, repository_, nullptr, &diffOptions);
			break;

		case DiffKind::IndexToIndex:
			return Diff();
			break;

		case DiffKind::IndexToCommit:
			newGitCommit = parseCommit(newRevision);
			errorCode = git_commit_tree(&newGitTree, newGitCommit);
			checkError(errorCode);

			errorCode = git_diff_tree_to_index(&gitDiff, repository_, newGitTree, nullptr, &diffOptions);
			reverseOldNew = true;
			break;

		case DiffKind::CommitToWorkdir:
			oldGitCommit = parseCommit(oldRevision);
			errorCode = git_commit_tree(&oldGitTree, oldGitCommit);
			checkError(errorCode);

			errorCode = git_diff_tree_to_workdir_with_index(&gitDiff, repository_, oldGitTree, &diffOptions);
			break;

		case DiffKind::CommitToIndex:
			oldGitCommit = parseCommit(oldRevision);
			errorCode = git_commit_tree(&oldGitTree, oldGitCommit);
			checkError(errorCode);

			errorCode = git_diff_tree_to_index(&gitDiff, repository_, oldGitTree, nullptr, &diffOptions);
			break;

		case DiffKind::CommitToCommit:
			oldGitCommit = parseCommit(oldRevision);
			errorCode = git_commit_tree(&oldGitTree, oldGitCommit);
			checkError(errorCode);

			newGitCommit = parseCommit(newRevision);
			errorCode = git_commit_tree(&newGitTree, newGitCommit);
			checkError(errorCode);

			errorCode = git_diff_tree_to_tree(&gitDiff, repository_, oldGitTree, newGitTree, &diffOptions);
			break;

		default:
			Q_ASSERT(false);

	}
	checkError(errorCode);

	// Use callback on diff to extract node information -> oldNodes & newNodes
	GitDiffExtract carryAlongData;
	QString oldCommitSHA1 = getSHA1(oldRevision);
	carryAlongData.oldTree_ = new GenericTree("oldDiff", oldCommitSHA1);
	QString newCommitSHA1 = getSHA1(newRevision);
	carryAlongData.newTree_ = new GenericTree("newDiff", newCommitSHA1);

	carryAlongData.reverseOldNew_ = reverseOldNew;
	git_diff_foreach(gitDiff, gitDiffExtractFileCallBack, NULL, gitDiffExtractLineCallBack, &(carryAlongData));

	// clean up
	git_commit_free(oldGitCommit);
	git_commit_free(newGitCommit);
	git_tree_free(oldGitTree);
	git_tree_free(newGitTree);
	git_diff_free(gitDiff);

	return Diff(carryAlongData.oldNodes_, carryAlongData.oldTree_,
					carryAlongData.newNodes_, carryAlongData.newTree_,
					this);
}

CommitGraph GitRepository::commitGraph(QString startRevision, QString endRevision) const
{
	git_commit* gitStartCommit = parseCommit(startRevision);
	const git_oid* startOID = git_commit_id(gitStartCommit);
	char* sha1 = git_oid_allocfmt(startOID);
	QString startSHA1(sha1);
	delete sha1;

	git_commit* gitEndCommit = parseCommit(endRevision);
	const git_oid* endOID = git_commit_id(gitEndCommit);
	sha1 = git_oid_allocfmt(endOID);
	QString endSHA1(sha1);
	delete sha1;

	CommitGraph graph(startSHA1, endSHA1);
	traverseCommitGraph(&graph, gitEndCommit, startOID);

	git_commit_free(gitStartCommit);
	git_commit_free(gitEndCommit);
	return graph;
}

Commit GitRepository::getCommit(QString revision) const
{
	Q_ASSERT(revision.compare(WORKDIR) != 0);
	Q_ASSERT(revision.compare(INDEX) != 0);

	int errorCode = 0;

	git_commit* gitCommit = parseCommit(revision);
	git_tree* tree = nullptr;
	errorCode = git_commit_tree(&tree, gitCommit);
	checkError(errorCode);

	GitCommitExtract treeWalkData;
	treeWalkData.repository_ = repository_;

	errorCode = git_tree_walk(tree, GIT_TREEWALK_PRE, treeWalkCommitExtractCallBack, &treeWalkData);
	checkError(errorCode);

	CommitMetaData info = getCommitInformation(revision);

	git_commit_free(gitCommit);
	git_tree_free(tree);

	Commit commit(info, treeWalkData.files_);
	return commit;
}

CommitFile GitRepository::getCommitFile(QString revision, QString relativePath) const
{
	if (revision.compare(WORKDIR) == 0)
		return getCommitFileFromWorkdir(relativePath);
	else if (revision.compare(INDEX) == 0)
		return getCommitFileFromIndex(relativePath);
	else
		return getCommitFileFromTree(revision, relativePath);
}

CommitMetaData GitRepository::getCommitInformation(QString revision) const
{
	CommitMetaData info;

	if (revision.compare(WORKDIR) != 0 && revision.compare(INDEX) != 0)
	{
		git_commit* gitCommit = parseCommit(revision);

		// SHA1
		const git_oid* oid = git_commit_id(gitCommit);
		char* sha1 = git_oid_allocfmt(oid);
		info.sha1_ = QString(sha1);
		delete sha1;

		// Message
		const char* msg = git_commit_message(gitCommit);
		info.message_ = QString(msg);

		// Date and Time
		git_time_t time = git_commit_time(gitCommit);
		info.dateTime_.setTime_t(time);

		// Committer
		Signature committer;
		const git_signature* gitCommitter = git_commit_committer(gitCommit);
		committer.name_ = QString(gitCommitter->name);
		committer.eMail_ = QString(gitCommitter->email);
		info.committer_ = committer;

		// Author
		Signature author;
		const git_signature* gitAuthor = git_commit_author(gitCommit);
		author.name_ = QString(gitAuthor->name);
		author.eMail_ = QString(gitAuthor->email);
		info.author_ = author;

		git_commit_free(gitCommit);
	}

	return info;
}

QString GitRepository::getSHA1(QString revision) const
{
	git_commit* gitCommit = parseCommit(revision);

	const git_oid* oid = git_commit_id(gitCommit);
	char* sha1 = git_oid_allocfmt(oid);
	QString commitSHA1(sha1);
	delete sha1;
	git_commit_free(gitCommit);

	return commitSHA1;
}

void GitRepository::checkout(QString revesion, bool force)
{
	if (revesion.compare(WORKDIR) != 0)
	{
		git_checkout_options options;
		git_checkout_init_options(&options, GIT_CHECKOUT_OPTIONS_VERSION);

		options.checkout_strategy = force ? GIT_CHECKOUT_FORCE : GIT_CHECKOUT_SAFE;

		int errorCode = 0;
		if (revesion.compare(INDEX) == 0)
			errorCode = git_checkout_index(repository_, nullptr, &options);
		else
		{
			git_commit* gitCommit = parseCommit(revesion);
			errorCode = git_checkout_tree(repository_, (git_object*)gitCommit, &options);

			git_commit_free(gitCommit);
		}
		checkError(errorCode);
	}
}


// Private methods

void GitRepository::traverseCommitGraph(CommitGraph* graph, git_commit* current, const git_oid* target) const
{
	const git_oid* oid = git_commit_id(current);
	char* sha1 = git_oid_allocfmt(oid);
	QString currentSHA1(sha1);
	delete sha1;

	if (git_oid_cmp(oid, target) != 0)
	{
		unsigned int numParents = git_commit_parentcount(current);
		for (unsigned int i = 0; i < numParents; i++)
		{
			git_commit* parent = nullptr;
			int errorCode = git_commit_parent(&parent, current, i);
			checkError(errorCode);

			const git_oid* parentOID = git_commit_id(parent);
			int parentIsTarget = git_oid_cmp(parentOID, target);
			int isConnected = git_graph_descendant_of(repository_, parentOID, target);
			if (isConnected == 1 || parentIsTarget == 0) {
				sha1 = git_oid_allocfmt(parentOID);
				QString parentSHA1(sha1);
				delete sha1;

				graph->add(parentSHA1, currentSHA1);

				traverseCommitGraph(graph, parent, target);
			}
			else if (isConnected != 0)
				checkError(isConnected);

			git_commit_free(parent);
		}
	}
}

CommitFile GitRepository::getCommitFileFromWorkdir(QString relativePath) const
{
	QString fullRelativePath = path_;
	fullRelativePath.append(relativePath);

	if (!QFile::exists(fullRelativePath))
		return CommitFile();

	QFile file(fullRelativePath);
	if ( !file.open(QIODevice::ReadOnly) )
		throw FilePersistenceException("Could not open file " + file.fileName() + ". " + file.errorString());

	Q_ASSERT(file.size() < std::numeric_limits<int>::max());
	int totalFileSize = static_cast<int>(file.size());

	auto mapped = reinterpret_cast<char*>(file.map(0, totalFileSize));
	Q_ASSERT(mapped);

	CommitFile commitFile(relativePath, totalFileSize, mapped);
	file.close();

	return commitFile;
}

CommitFile GitRepository::getCommitFileFromIndex(QString relativePath) const
{
	// check if such a file is in the index
	git_index* index = nullptr;
	int errorCode = git_repository_index(&index, repository_);
	checkError(errorCode);

	qDebug() << relativePath;
	const char* path = relativePath.toStdString().c_str();
	const git_index_entry* entry = git_index_get_bypath(index, path, 0);

	if (entry == nullptr)
	{
		return CommitFile();
	}

	git_checkout_options options;
	git_checkout_init_options(&options, GIT_CHECKOUT_OPTIONS_VERSION);

	// only checkout the file with relativePath
	git_strarray pathArray;
	pathArray.count = 1;
	pathArray.strings = new char*[1];
	pathArray.strings[0] = new char[relativePath.size()];
	memcpy(pathArray.strings[0], path, relativePath.size());
	options.paths = pathArray;

	options.checkout_strategy = GIT_CHECKOUT_FORCE;

	// make a copy of current workdir file
	QString fullRelativePath = path_;
	fullRelativePath.append(relativePath);
	qDebug() << path_;
	QFile workdirFile(fullRelativePath);
	QString copyName = fullRelativePath;
	copyName.append("_COPY_FOR_INDEX_CHECKOUT");
	bool success = workdirFile.copy(copyName);
	Q_ASSERT(success);

	qDebug() << copyName;
	workdirFile.close();

	errorCode = git_checkout_index(repository_, nullptr, &options);
	checkError(errorCode);

	QFile file(fullRelativePath);
	if ( !file.open(QIODevice::ReadOnly) )
		throw FilePersistenceException("Could not open file " + file.fileName() + ". " + file.errorString());

	Q_ASSERT(file.size() < std::numeric_limits<int>::max());
	int totalFileSize = static_cast<int>(file.size());

	auto mapped = reinterpret_cast<char*>(file.map(0, totalFileSize));
	Q_ASSERT(mapped);

	CommitFile commitFile(relativePath, totalFileSize, mapped);

	qDebug() << fullRelativePath;

	success = file.remove();
	Q_ASSERT(success);
	success = QFile::copy(copyName, fullRelativePath);
	Q_ASSERT(success);
	success = QFile::remove(copyName);
	Q_ASSERT(success);

	qDebug() << commitFile.content_;

	qDebug() << "END OF METHOD";

	// FIXME after return commitFile has no content!

	return commitFile;
}

CommitFile GitRepository::getCommitFileFromTree(QString revision, QString relativePath) const
{
	Q_ASSERT(revision.compare(WORKDIR) != 0);
	Q_ASSERT(revision.compare(INDEX) != 0);

	int errorCode = 0;

	git_commit* gitCommit = parseCommit(revision);
	git_tree* tree = nullptr;
	errorCode = git_commit_tree(&tree, gitCommit);
	checkError(errorCode);

	git_tree_entry* treeEntry = nullptr;
	errorCode = git_tree_entry_bypath(&treeEntry, tree, relativePath.toStdString().c_str());
	if (errorCode == GIT_ENOTFOUND)
		return CommitFile();
	checkError(errorCode);

	git_object* obj = nullptr;
	errorCode = git_tree_entry_to_object(&obj, repository_, treeEntry);
	checkError(errorCode);

	CommitFile file;
	if (git_object_type(obj) == GIT_OBJ_BLOB)
	{
		file.relativePath_ = relativePath;

		git_blob* blob = (git_blob*)obj;

		qint64 contentSize = git_blob_rawsize(blob);
		file.size_ = contentSize;

		const char* rawContent = (const char*)git_blob_rawcontent(blob);
		char* content = new char[contentSize];
		memcpy(content, rawContent, contentSize);
		file.content_ = content;
	}

	git_commit_free(gitCommit);
	git_tree_free(tree);
	git_tree_entry_free(treeEntry);
	git_object_free(obj);

	return file;
}

git_commit* GitRepository::parseCommit(QString revision) const
{
	int errorCode = 0;
	git_object* obj = nullptr;

	errorCode = git_revparse_single(&obj, repository_, revision.toStdString().c_str());

	switch (errorCode)
	{
		// FIXME pass errors to the next level
		case GIT_ENOTFOUND:
			std::cout << "Error: " << revision.toStdString().c_str() << " not found!" << std::endl;
			Q_ASSERT(false);

		case GIT_EAMBIGUOUS:
			std::cout << "Error: " << revision.toStdString().c_str() << " is ambiguous!" << std::endl;
			Q_ASSERT(false);

		case GIT_EINVALIDSPEC:
			std::cout << "Error: " << revision.toStdString().c_str() << " is invalid!" << std::endl;
			Q_ASSERT(false);

		default:
			checkError(errorCode);
	}

	if (git_object_type(obj) == GIT_OBJ_TAG)
	{
		git_tag* tag = (git_tag*)obj;
		git_object* dereferencedTarget = nullptr;
		errorCode = git_tag_peel(&dereferencedTarget, tag);
		checkError(errorCode);
		// free unused objects
		git_tag_free(tag);

		obj = dereferencedTarget;
	}

	if (git_object_type(obj) == GIT_OBJ_COMMIT)
		return (git_commit*)obj;
	else
	{
		std::cout << "Error: " << revision.toStdString().c_str() << " is not a commit!" << std::endl;
		Q_ASSERT(false);
	}

	// clean up
	git_object_free(obj);
}

GitRepository::DiffKind GitRepository::kind(QString oldRevision, QString newRevision)
{
	DiffKind diffKind = DiffKind::Unspecified;

	if (oldRevision.compare(WORKDIR) == 0)
	{
		if (newRevision.compare(WORKDIR) == 0)
			diffKind = DiffKind::WorkdirToWorkdir;
		else if (newRevision.compare(INDEX) == 0)
			diffKind = DiffKind::WorkdirToIndex;
		else
			diffKind = DiffKind::WorkdirToCommit;
	}
	else if (oldRevision.compare(INDEX) == 0)
	{
		if (newRevision.compare(WORKDIR) == 0)
			diffKind = DiffKind::IndexToWorkdir;
		else if (newRevision.compare(INDEX) == 0)
			diffKind = DiffKind::IndexToIndex;
		else
			diffKind = DiffKind::IndexToCommit;
	}
	else
	{
		if (newRevision.compare(WORKDIR) == 0)
			diffKind = DiffKind::CommitToWorkdir;
		else if (newRevision.compare(INDEX) == 0)
			diffKind = DiffKind::CommitToIndex;
		else
			diffKind = DiffKind::CommitToCommit;
	}

	Q_ASSERT(diffKind != DiffKind::Unspecified);
	return diffKind;
}

void GitRepository::checkError(int errorCode)
{
	if (errorCode < 0)
	{
		const git_error* lastError = giterr_last();
		qDebug() << "Error " << errorCode << "/" << lastError->klass << ": " << lastError->message;
		exit(errorCode);
	}
}

} /* namespace FilePersistence */
