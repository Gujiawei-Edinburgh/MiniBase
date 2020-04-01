#ifndef BTINDEX_PAGE_H
#define BTINDEX_PAGE_H


#include "minirel.h"
#include "page.h"
#include "sortedpage.h"
#include "bt.h"



class BTIndexPage : public SortedPage {
	
private:

	// No private or public members should be declared.

public:
	
	// You may add public methods here.
	
	Status Insert(const int key, const PageID pid, RecordID& rid);
	Status Delete(const int key, RecordID& rid);

	Status GetFirst(int& key, PageID& pid, RecordID& rid);
	Status GetNext(int& key, PageID& pid, RecordID& rid);
	
	PageID GetLeftLink(void);
	void SetLeftLink(PageID left);
	    
	IndexEntry* GetEntry(int slotNo) 
	{
		return (IndexEntry *)(data + slots[slotNo].offset);
	}

	bool IsAtLeastHalfFull()
	{
		return (AvailableSpace() <= (HEAPPAGE_DATA_SIZE) / 2);
	}
	Status GetPageID (const int *key, PageID& pid);
	int KeyCmp(const int* key1, const int* key2);
	Status GetKeyData(int& key, PageID& pid, RecordID& rid);
	Status DeletePage (PageID pid, bool rightSibling);
	Status FindSiblingForChild(PageID targetPid, PageID& siblingPid, bool& rightSibling);
	Status FindKey (int& key, int& entry);
	Status GetLast (RecordID& rid, int key, PageID & pageNo);
	Status AdjustKey (int& newKey, int& oldKey);
	Status FindPage(int key, PageID& pageNo, bool& leftMost);
	Status FindKeyWithPage(PageID pageNo, int& key, bool& leftMost);
	Status UpdateKey(PageID targetPid,int key);
};

#endif
