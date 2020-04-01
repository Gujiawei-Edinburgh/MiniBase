#include "minirel.h"
#include "bufmgr.h"
#include "db.h"
#include "new_error.h"
#include "btfile.h"
#include "btfilescan.h"

//-------------------------------------------------------------------
// BTreeFileScan::~BTreeFileScan
//
// Input   : None
// Output  : None
// Purpose : Clean up the B+ tree scan.
//-------------------------------------------------------------------

BTreeFileScan::~BTreeFileScan()
{
    // TODO: add your code here
}


//-------------------------------------------------------------------
// BTreeFileScan::GetNext
//
// Input   : None
// Output  : rid  - record id of the scanned record.
//           key  - key of the scanned record
// Purpose : Return the next record from the B+-tree index.
// Return  : OK if successful, DONE if no more records to read.
//-------------------------------------------------------------------

Status 
BTreeFileScan::GetNext(RecordID& rid, int& keyPtr)
{
    // TODO: add your code here 
	cout<<"is scan finished :"<<scanFinished<<endl;
    if (scanFinished) return DONE;

	RecordID dataRid;
	BTLeafPage *curPage;

	if(!scanStarted) {
		// Find the record on the leftmost leaf page where we are supposed to start
		cout<<"if not started"<<endl;
		curPageID = leftmostLeafID;
		cout<<"leftmostLeafID:"<<leftmostLeafID<<endl;
		PIN(curPageID, curPage);
		cout<<"Finish PIN"<<endl;
		// Find the first non-empty page
		while (curPage->GetFirst(keyPtr,dataRid,curRid) != OK) {
			PageID nextPageID = curPage->GetNextPage();
			UNPIN(curPageID, CLEAN);

			if (nextPageID == INVALID_PAGE) {
				scanFinished = true;
				return DONE;
			}
			
			curPageID = nextPageID;
			PIN(curPageID, curPage);
		}
		

		// Case where the lowKey is NULL (We want to start at the first spot)
		if (lowKey == NULL) {
			if (highKey == NULL || KeyCmp(&keyPtr,highKey) <= 0) {
				rid = dataRid;
				scanStarted = true;
				UNPIN(curPageID, CLEAN);
				return OK;
			}
			else {
				scanFinished = true;
				UNPIN(curPageID, CLEAN);
				return DONE;
			}
		}
		// lowKey is not NULL
		else {
			if (KeyCmp(lowKey,&keyPtr) <= 0) {
				if (highKey == NULL || KeyCmp(&keyPtr,highKey) <= 0) {
					rid = dataRid;
					scanStarted = true;
					UNPIN(curPageID, CLEAN);
					return OK;
				}
				else {
					scanFinished = true;
					UNPIN(curPageID, CLEAN);
					return DONE;
				}
			}
			else{
				// Get the next record and try again
				cout<<*lowKey<<endl;
				while (curPage->GetNext(keyPtr,dataRid,curRid) != DONE){
					if (KeyCmp(lowKey,&keyPtr) <= 0) {
						if (highKey == NULL || KeyCmp(&keyPtr,highKey) <= 0) {
							rid = dataRid;
							scanStarted = true;
							UNPIN(curPageID, CLEAN);
							return OK;
						}
						else {
							scanFinished = true;
							UNPIN(curPageID, CLEAN);
							return DONE;
						}
					}
				}
				scanFinished = true;
				UNPIN(curPageID, CLEAN);
				return DONE;
			}	
		}
	}
	// The scan has allready been initialized
	else {
		// PIN our current page
		PIN(curPageID, curPage);
		Status s = curPage->GetNext(keyPtr, dataRid, curRid);

		// See if we have more records on that page, if not we need to get the next page
		while (s == DONE && curPage->GetNextPage() != INVALID_PAGE) {
			PageID nextPageID = curPage->GetNextPage();
			UNPIN(curPageID, CLEAN);
			curPageID = nextPageID;

			PIN(curPageID, curPage);
			s = curPage->GetFirst(keyPtr, dataRid, curRid);
		}

		// Check if we are done the scan
		if (s == DONE && curPage->GetNextPage() == INVALID_PAGE) {
				UNPIN(curPageID, CLEAN);
				scanFinished = true;
				return DONE;
		}
		// If we are not done, curRid, keyPtr, and dataRid are all propperly set
		else{
			if (highKey == NULL || KeyCmp(&keyPtr,highKey) <= 0) {
				rid = dataRid;
				UNPIN(curPageID, CLEAN);
				return OK;
			}
			else {
				scanFinished = true;
				UNPIN(curPageID, CLEAN);
				return DONE;
			}
		}

	}
}


//-------------------------------------------------------------------
// BTreeFileScan::DeleteCurrent
//
// Input   : None
// Output  : None
// Purpose : Delete the entry currently being scanned (i.e. returned
//           by previous call of GetNext())
// Return  : OK if successful, DONE if no more record to read.
//-------------------------------------------------------------------


Status 
BTreeFileScan::DeleteCurrent()
{  
    // TODO: add your code here
    return OK;
}

void
BTreeFileScan::Init(const int* low, const int* high, PageID leftmostLeafPageID)
{
    this->lowKey = low;
	this->highKey = high;
	this->leftmostLeafID = leftmostLeafPageID;
	this->curPageID = NULL;
	scanStarted = false;
	scanFinished = false;

	if (leftmostLeafPageID == INVALID_PAGE) scanFinished = true;
}

int
BTreeFileScan::KeyCmp(const int* key1, const int* key2)
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


