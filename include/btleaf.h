#ifndef BTLEAF_PAGE_H
#define BTLEAF_PAGE_H

#include "minirel.h"
#include "page.h"
#include "sortedpage.h"
#include "bt.h"
#include "btindex.h"


class BTLeafPage : public SortedPage {
	
private:
	
	// No private or public members should be declared.
	
public:
		
	Status Insert(const int key, const RecordID dataRid, RecordID& rid);
	Status Delete(const int key, const RecordID dataRid, RecordID& rid);
	
	Status GetFirst(int& key, RecordID& dataRid, RecordID& rid);
	Status GetNext(int& key, RecordID& dataRid, RecordID& rid);
	Status GetCurrent(int& key, RecordID& dataRid, RecordID rid);
	Status GetLast (RecordID& rid, int& key, RecordID & pageNo);
	
	LeafEntry* GetEntry(int slotNo)
	{
		return (LeafEntry *)(data + slots[slotNo].offset);
	}

	bool IsAtLeastHalfFull()
	{
		return (AvailableSpace() <= (HEAPPAGE_DATA_SIZE) / 2);
	}

	bool CanBorrow()
	{
		return (AvailableSpace() <= ((HEAPPAGE_DATA_SIZE) / 2)-1);
	}

};

#endif
