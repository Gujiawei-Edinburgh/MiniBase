#ifndef _BTREE_FILESCAN_H
#define _BTREE_FILESCAN_H

#include "btfile.h"

class BTreeFile;

class BTreeFileScan : public IndexFileScan {
	
public:
	
	friend class BTreeFile;

	Status GetNext(RecordID& rid,  int& key);
	Status DeleteCurrent();
	Status _SetIter();
	void Init(const int* low, const int* high, PageID leftmostLeafPageID);
	int KeyCmp(const int* key1, const int* key2);

	~BTreeFileScan();
	
private:
	const int* lowKey = NULL;
	const int* highKey = NULL;
	BTLeafPage* curPage;
	PageID curPageID;
	PageID leftmostLeafID;
	RecordID curRid;
	bool scanStarted;
	bool scanFinished;
};

#endif
