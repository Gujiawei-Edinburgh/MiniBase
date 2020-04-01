#ifndef _BTFILE_H
#define _BTFILE_H

#include "btindex.h"
#include "btleaf.h"
#include "index.h"
#include "btfilescan.h"
#include "bt.h"

enum SplitStatus {
	NEEDS_SPLIT,
	CLEAN_INSERT
};

class BTreeFile: public IndexFile {
	
public:
	
	friend class BTreeFileScan;

	BTreeFile(Status& status, const char* filename);
	~BTreeFile();
	
	Status DestroyFile();
	
	Status Insert(const int key, const RecordID rid); 
	Status Delete(const int key, const RecordID rid);
	Status Delete1(const int key, const RecordID rid);
	Status Delete2(const int key, const RecordID rid);
    
    
	IndexFileScan* OpenScan(const int* lowKey, const int* highKey);
	
	Status Print();
	Status DumpStatistics();
	Status Search(const int* key,  PageID& foundPid);

private:
	
	// You may add members and methods here.

	PageID rootPid;
	const char* fileName;

	Status PrintTree(PageID pid);
	Status PrintNode(PageID pid);
	Status DestoryHelper(PageID pid);
	Status _searchTree( const int* key,  PageID currentID, PageID& lowIndex);
	PageID GetLeftLeaf();
	Status _Search( const int* key,  PageID, PageID&);
	Status _SearchIndex (const int* key,  PageID currIndexID, BTIndexPage *currIndex, PageID& foundID);
	Status SplitLeafNode(const int key, const RecordID rid, BTLeafPage *fullPage, PageID &newPageID, int &newPageFirstKey);
	Status SplitIndexNode(const int key, const PageID pid, BTIndexPage *fullPage, PageID &newPageID, int &newPageFirstKey);
	int GetKeyDataLength(const int key, const NodeType nodeType);
	int KeyCmp(const int key1, const int key2);
	Status _DumpStatistics(PageID pageID);
	Status DumpHelper (PageID pageID);
	Status PrintTree2( PageID pageID, int option);
	Status _PrintTree ( PageID pageID);
	Status _Delete(PageID parentPid, PageID nodePid, int key, const RecordID rid, PageID& oldPid, bool& rightSibling);

	struct BTreeHeaderPage : HeapPage {
	public:
		// Initializes the header page and sets the root to be invalid.
		void Init(PageID hpid) {
			HeapPage::Init(hpid);
			SetRootPageID(INVALID_PAGE);
		}
		PageID GetRootPageID() {
			return *((PageID *) HeapPage::data);
		}
		// Sets the page id of the root.
		void SetRootPageID(PageID pid) {
			PageID *ptr = (PageID *)(HeapPage::data);
			*ptr = pid;
		}
    };
	BTreeHeaderPage *header;
	PageID headerID;
	int				totalDataPages;
	int				totalIndexPages;
	float				maxDataFillFactor;
	float				minDataFillFactor;
	float				maxIndexFillFactor;
	float				minIndexFillFactor;
	float				totalFillData; // sum of each data nodes' usedspace/fullpagespace
	float				totalFillIndex;
	int				totalNumIndex; // total num of Index Entries
	int				totalNumData;
	int				hight; // hight of Tree
};


#endif // _BTFILE_H
