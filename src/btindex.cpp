#include <string.h>
#include "btindex.h"


//-------------------------------------------------------------------
// BTIndexPage::Insert
//
// Input   : key - value of the key to be inserted.
//           pageID - page id associated to that key.
// Output  : rid - record id of the (key, pageID) record inserted.
// Purpose : Insert the pair (key, pageID) into this index node.
// Return  : OK if insertion is succesfull, FAIL otherwise.
//-------------------------------------------------------------------

Status 
BTIndexPage::Insert(const int key, const PageID pageID, RecordID& rid)
{
	IndexEntry entry;
	entry.key = key;
	entry.pid = pageID;

	Status s = SortedPage::InsertRecord((char *)&entry, sizeof(IndexEntry), rid);
	if (s != OK)
	{
		cerr << "Fail to insert record into IndexPage" << endl;
		return FAIL;
	}
	
	return OK;
}


//-------------------------------------------------------------------
// BTIndexPage::Delete
//
// Input   : key  - value of the key to be deleted.
// Output  : rid - record id of the (key, pageID) record deleted.
// Purpose : Delete the entry associated with key from this index node.
// Return  : OK if deletion is succesfull, FAIL otherwise. If FAIL is
//           returned, rid may contain garbage.
//-------------------------------------------------------------------

Status 
BTIndexPage::Delete (const int key, RecordID& rid)
{
	// Scan through all the records in this page and find the
	// matching pair (key, dataRid).

	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		IndexEntry* entry = (IndexEntry *)(data + slots[i].offset);
		if (entry->key == key)
		{
			// We delete it here.
			rid.pageNo = PageNo();
			rid.slotNo = i;
			Status s = SortedPage::DeleteRecord(rid);
			return s;
		}
	}
	
	return FAIL;
}


//-------------------------------------------------------------------
// BTIndexPage::GetFirst
//
// Input   : None
// Output  : rid - record id of the first entry
//           firstKey - pointer to the key value
//           firstPid - the page id
// Purpose : get the first pair (firstKey, firstPid) in the index page 
//           and it's rid.
// Return  : OK if a record is returned,  DONE if no record exists on 
//           this page.
//-------------------------------------------------------------------

Status 
BTIndexPage::GetFirst(int& firstKey, PageID& firstPid, RecordID& rid)
{
	// Initialize the record id of the first (key, dataRid) pair.  The
	// first record is always at slot position 0, since SortedPage always
	// compact it's records.  We can also use HeapPage::FirstRecord here
	// but it is not neccessary.

	rid.pageNo = pid;
	rid.slotNo = 0;

	// If there are no record in this page, just return DONE.
	
	if (numOfSlots == 0)
	{
		rid.pageNo = INVALID_PAGE;
		rid.slotNo = INVALID_SLOT;
		return DONE;
	}
	
	// Otherwise, we just copy the record into key and dataRid,
	// and returned.  The record is located at offset slots[0].offset
	// from the beginning of data area (pointed to by member data in 
	// HeapPage)

	IndexEntry entry;
	memcpy(&entry, (IndexEntry *)(data + slots[0].offset), sizeof(IndexEntry));
	firstKey = entry.key;
	firstPid = entry.pid;
	
	return OK;
}


//-------------------------------------------------------------------
// BTIndexPage::GetNext
//
// Input   : rid - record id of the current entry
// Output  : rid - record id of the next entry
//           nextKey - the key value of next entry
//           nextPid - the page id of next entry
// Purpose : Get the next pair (nextKey, nextPid) in the index page and 
//           it's rid.
// Return  : OK if there is a next record, DONE if no more.  If DONE is
//           returned, rid is set to invalid.
//-------------------------------------------------------------------

Status 
BTIndexPage::GetNext(int& nextKey, PageID& nextPid, RecordID& rid)
{
	// If we are at the end of records, return DONE.

	if (rid.slotNo + 1 >= numOfSlots)
	{
		rid.pageNo = INVALID_PAGE;
		rid.slotNo = INVALID_SLOT;
		return DONE;
	}

	// Increment the slotNo in rid to point to the next record in this
	// page.  We can do this for subclass of SorterPage since the records
	// in a sorted page is always compact.  
	
	rid.slotNo++;

	// Otherwise, we just copy the record into key and dataRid,
	// and returned.  The record is located at offset 
	// slots[rid.slotNo].offset from the beginning of data area 
	// (pointed to by member data in HeapPage)

	IndexEntry entry;
	memcpy(&entry, (IndexEntry *)(data + slots[rid.slotNo].offset), sizeof(IndexEntry));
	nextKey = entry.key;
	nextPid = entry.pid;
	
	return OK;
}


//-------------------------------------------------------------------
// BTIndexPage::GetLeftLink
//
// Input   : None
// Output  : None
// Purpose : Return the page id of the page at the left of this page.
// Return  : The page id of the page at the left of this page.
//-------------------------------------------------------------------

PageID BTIndexPage::GetLeftLink()
{
	return GetPrevPage();
}


//-------------------------------------------------------------------
// BTIndexPage::SetLeftLink
//
// Input   : pageID - new left link
// Output  : None
// Purpose : Set the page id of the page at the left of this page.
// Return  : None
//-------------------------------------------------------------------

void BTIndexPage::SetLeftLink(PageID pageID)
{
	SetPrevPage(pageID);
}

Status BTIndexPage::GetPageID (const int *key, PageID& pid)
{
	// IndexEntry *currKey;
	IndexEntry entry;
	// A sequential search is implemented here.  You can modify it
	// to a binary search if you like.
	
	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		// currKey = (IndexEntry*)(data + slots[i].offset);
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		
		if (KeyCmp(key, &(entry.key)) >= 0)
		{
			key = &(entry.key);
			pid = entry.pid;
			return OK;
		}
	}
	
	// If we reach this point, then the page we should follow in our 
	// B+tree search must be the leftmost child of this page.
	
	pid = GetLeftLink();
	return OK;
}

int BTIndexPage::KeyCmp(const int* key1, const int* key2)
{
	if (*key1 < *key2)
	{
		return -1;
	}
	else if (*key1 == *key2)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

Status BTIndexPage::GetKeyData(int& key, PageID& pid, RecordID& rid)
{
	if (rid.slotNo >= numOfSlots)
	{
		rid.pageNo = INVALID_PAGE;
		rid.slotNo = INVALID_SLOT;
		return DONE;
	}
	IndexEntry entry;
	memcpy(&entry, (IndexEntry *)(data + slots[rid.slotNo].offset), sizeof(IndexEntry));
	key = entry.key;
	pid = entry.pid;
	
	return OK;
}

Status BTIndexPage::DeletePage (PageID pid, bool rightSibling)
{
	PageID pageNo;
	int currKey = 0;
	RecordID rid;
	Status s;

	if (GetPrevPage() == pid) {
		s = GetFirst (currKey, pageNo, rid);
		Delete(currKey, rid);
		SetLeftLink(pageNo);
		currKey = 0;
		return OK;
	} else {
		s = GetFirst (currKey, pageNo, rid);
		if (pageNo == pid && rightSibling) {
			Delete(currKey, rid);
			currKey = 0;
			return OK;
		}

		while (pageNo != pid)
		{
			s = GetNext (currKey, pageNo, rid);
			if (s != OK) {
				pid = INVALID_PAGE;
				break;
			}
		}

		if (pid == INVALID_PAGE) {
			currKey = 0;
			return FAIL;
		} else {
			PageID targetPid = pageNo;
			int targetKey = 0;
			targetKey = currKey;

			s = GetNext(currKey, pageNo, rid);
			int oldKey = 0;
			oldKey = currKey;

			s = Delete(targetKey, rid);

			AdjustKey(targetKey, oldKey);
			currKey = 0;
			return OK;
		}

	}
}

Status BTIndexPage::FindSiblingForChild(PageID targetPid, PageID& siblingPid, bool& rightSibling)
{
	// if (GetLeftLink() == targetPid) {
	// 	rightSibling = true;
	// 	IndexEntry entry;
	// 	memcpy(&entry, (IndexEntry *)(data + slots[0].offset), sizeof(IndexEntry));
	// 	siblingPid = entry.pid;
	// }

	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		PageID pageNo;
		IndexEntry entry;
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		pageNo = entry.pid;
		if (targetPid == pageNo) {
			rightSibling = false;
			if (i == numOfSlots - 1) {
				IndexEntry entry;
				memcpy(&entry, (IndexEntry *)(data + slots[i-1].offset), sizeof(IndexEntry));
				siblingPid = entry.pid;
			} else {
				IndexEntry entry;
				memcpy(&entry, (IndexEntry *)(data + slots[i+1].offset), sizeof(IndexEntry));
				siblingPid = entry.pid;
				rightSibling = true;
			}
			return OK;
		}
	}

	return FAIL;
}

Status BTIndexPage::FindKey (int& key, int& entry)
{
	IndexEntry indexentry;
	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		memcpy(&indexentry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		
		if (KeyCmp(&key, &(indexentry.key)) >= 0)
		{
			entry = indexentry.key;
			return OK;
		}
	}
	return FAIL;
}

Status BTIndexPage::GetLast (RecordID& rid, int key, PageID & pageNo)
{
	if (numOfSlots == 0) 
	{
		pageNo = INVALID_PAGE;
		return DONE;
	}

	rid.pageNo = pid;
	rid.slotNo = numOfSlots - 1;

	IndexEntry entry;
	memcpy(&entry, (IndexEntry *)(data + slots[numOfSlots - 1].offset), sizeof(IndexEntry));
	key = entry.key;
	pageNo = entry.pid;
	return OK;
}

Status BTIndexPage::AdjustKey (int& newKey, int& oldKey)
{
	for (int i = numOfSlots -1; i >= 0; i--) {
		IndexEntry entry;
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
        if (KeyCmp(&oldKey, &(entry.key)) >= 0) {
			newKey = entry.key; 
			return OK;
        }
    }
    return FAIL;
}

Status BTIndexPage::FindPage(int key, PageID& pageNo, bool& leftMost)
{
	IndexEntry entry;
	
	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		
		if (KeyCmp(&key, &(entry.key)) >= 0)
		{
			key = entry.key;
			pid = entry.pid;
			leftMost = false;
			return OK;
		}
	}
	
	leftMost = true;
	pid = GetLeftLink();
	return OK;
}

Status BTIndexPage::FindKeyWithPage(PageID targetPid, int& key, bool& leftMost)
{
	if (GetLeftLink() == targetPid) {
		leftMost = true;
		return OK;
	}

	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		PageID pageNo;
		IndexEntry entry;
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		key = entry.key;
		pageNo = entry.pid;
		if (targetPid == pageNo) {
			leftMost = false;
			return OK;
		}
	}

	return FAIL;
}

Status BTIndexPage::UpdateKey(PageID targetPid,int key)
{
	for (int i = numOfSlots - 1; i >= 0; i--)
	{
		PageID pageNo;
		IndexEntry entry;
		memcpy(&entry, (IndexEntry *)(data + slots[i].offset), sizeof(IndexEntry));
		pageNo = entry.pid;
		if (targetPid == pageNo) {
			RecordID rid;
			// Delete(key,)
			return OK;
		}
	}

	return FAIL;
}

