#include "minirel.h"
#include "bufmgr.h"
#include "db.h"
#include "new_error.h"
#include "btfile.h"
#include "btfilescan.h"
#include <stack>


//-------------------------------------------------------------------
// BTreeFile::BTreeFile
//
// Input   : filename - filename of an index.  
// Output  : returnStatus - status of execution of constructor. 
//           OK if successful, FAIL otherwise.
// Purpose : If the B+ tree exists, open it.  Otherwise create a
//           new B+ tree index.
//-------------------------------------------------------------------

BTreeFile::BTreeFile (Status& returnStatus, const char* filename) 
{
    // TODO: add your code here
	this->fileName = strcpy(new char[strlen(filename) + 1], filename);

	Status stat = MINIBASE_DB->GetFileEntry(filename, headerID);
	Page *_headerPage;
	returnStatus = OK;

	// File does not exist, so we should create a new index file.
	if (stat == FAIL) {
		// Allocate a new header page.
		stat = MINIBASE_BM->NewPage(headerID, _headerPage);

		if (stat != OK) {
			cout << "Fail to allocate a new page" << endl;
			headerID = INVALID_PAGE;
			header = NULL;
			returnStatus = FAIL;
			return;
		}

		header = (BTreeHeaderPage *)(_headerPage);
		header->Init(headerID);
		stat = MINIBASE_DB->AddFileEntry(filename, headerID);

		if (stat != OK) {
			cout<<"Fail to create the file"<<endl;
			headerID = INVALID_PAGE;
			header = NULL;
			returnStatus = FAIL;
			return;
		}
	} else {
		stat = MINIBASE_BM->PinPage(headerID, _headerPage);

		if (stat != OK) {
			cout<<"Fail to pinn the page"<<endl;
			headerID = INVALID_PAGE;
			header = NULL;
			returnStatus = FAIL;
		}

		header = (BTreeHeaderPage *) _headerPage;
	}
}


//-------------------------------------------------------------------
// BTreeFile::~BTreeFile
//
// Input   : None 
// Output  : None
// Purpose : Clean Up
//-------------------------------------------------------------------

BTreeFile::~BTreeFile()
{
    // TODO: add your code here
	delete [] this->fileName;
	
    if (headerID != INVALID_PAGE) 
	{
		Status st = MINIBASE_BM->UnpinPage (headerID, CLEAN);
		if (st != OK)
		{
			cout<<"Deconstruction: Fail to unpin the page"<<endl;
		}
    }
}


//-------------------------------------------------------------------
// BTreeFile::DestroyFile
//
// Input   : None
// Output  : None
// Return  : OK if successful, FAIL otherwise.
// Purpose : Delete the entire index file by freeing all pages allocated
//           for this BTreeFile.
//-------------------------------------------------------------------

Status 
BTreeFile::DestroyFile()
{
    // TODO: add your code here
	if (header->GetRootPageID() != INVALID_PAGE && DestoryHelper(header->GetRootPageID()) != OK) {
		return FAIL;
	}
	FREEPAGE(headerID);
	headerID = INVALID_PAGE;
	header = NULL;

	if (MINIBASE_DB->DeleteFileEntry(this->fileName) != OK) {
		cout<<"Fail to delete the file entry"<<endl;
		return FAIL;
	}

	return OK;
}


Status
BTreeFile::DestoryHelper(PageID pageID)
{
	SortedPage *page;
	BTIndexPage *index;

	Status s;
	PageID curPageID;

	RecordID curRid;
	int key;

	PIN(pageID, page);
	NodeType type = (NodeType)page->GetType();

	switch (type) {
	case INDEX_NODE:
		index = (BTIndexPage *)page;
		curPageID = index->GetLeftLink();
		if (DestoryHelper(curPageID) != OK) {
			cout<<"Fail to destory file at destoryhelper"<<endl;
			return FAIL;
		}
		s=index->GetFirst(key,curPageID,curRid);
		if ( s == OK) {	
			if (DestoryHelper(curPageID) != OK) {
				cout<<"Fail to destory file at destoryhelper"<<endl;
				return FAIL;
			}
			s = index->GetNext(key,curPageID,curRid);
			while ( s != DONE) {	
				if (DestoryHelper(curPageID) != OK) {
					cout<<"Fail to destory file at destoryhelper"<<endl;
					return FAIL;
				}
				s = index->GetNext(key,curPageID,curRid);
			}
		}

		FREEPAGE(pageID);
		break;

	case LEAF_NODE:
		FREEPAGE(pageID);
		break;
	}

	return OK;
}


//-------------------------------------------------------------------
// BTreeFile::Insert
//
// Input   : key - the value of the key to be inserted.
//           rid - RecordID of the record to be inserted.
// Output  : None
// Return  : OK if successful, FAIL otherwise.
// Purpose : Insert an index entry with this rid and key.
// Note    : If the root didn't exist, create it.
//-------------------------------------------------------------------


Status 
BTreeFile::Insert(const int key, const RecordID rid)
{
    // TODO: add your code here
	RecordID newRecordID;
	//Two cases: (1) Null of root (2)root is a node 2.1 leaf 2.2 index node root node is a special case
	if (header->GetRootPageID() == INVALID_PAGE) {
		// There is no root page, we need to make one
		PageID newPageID;
		Page *newPage;

		NEWPAGE(newPageID, newPage);

		BTLeafPage *newLeafPage = (BTLeafPage *) newPage;
		newLeafPage->Init(newPageID);
		newLeafPage->SetType(LEAF_NODE);

		if (newLeafPage->Insert(key, rid, newRecordID) != OK) {
			FREEPAGE(newPageID);
			return FAIL;
		}

		header->SetRootPageID(newPageID);

		UNPIN(newPageID, DIRTY);
	}
	else
	{
		// A root page exists
		PageID rootID = header->GetRootPageID();
		SortedPage *rootPage;
		PIN(rootID, rootPage);
		Status s;

		// The rootPage is a leaf node, this is the trivial case
		if (rootPage->GetType() == LEAF_NODE) {
            BTLeafPage* rootleaf = (BTLeafPage*) rootPage;

			// See if there is space in the root leaf to insert the new key
            if (rootPage->AvailableSpace() >= GetKeyDataLength(key,LEAF_NODE)) {
                if(rootleaf->Insert(key, rid, newRecordID) != OK) {
					UNPIN(rootID, CLEAN);
					return FAIL;
				}
                UNPIN(rootID, DIRTY);
            }

			//root page is a leaf node and need to be splitted
            else {
				PageID newPageID;
				int newPageFirstKey;

				if (SplitLeafNode(key, rid, rootleaf, newPageID, newPageFirstKey) != OK){
					UNPIN (rootID, CLEAN);
					return FAIL;
				}

				PageID newIndexPageID;
				Page *newIndexPage;
                
				NEWPAGE(newIndexPageID, newIndexPage);
				
				// Create the new root index node and copy up key value into it
				BTIndexPage *rootindex = (BTIndexPage *) newIndexPage;
				rootindex->Init(newIndexPageID);
				rootindex->SetType(INDEX_NODE);
				rootindex->SetPrevPage(rootID);

				if (rootindex->Insert(newPageFirstKey, newPageID, newRecordID) != OK) {
					UNPIN(newIndexPageID, CLEAN);
					UNPIN(rootID, DIRTY);
					return FAIL;
				}
                header->SetRootPageID(newIndexPageID);
				UNPIN(rootID, DIRTY);
                UNPIN(newIndexPageID, DIRTY);
            }
        }
		else {
			// The root page is an index node so we will have to traverse through the tree to find the leaf page to insert on
            SortedPage* curPage = rootPage;
			PageID curIndexID;
			PageID nextPageID;
			Status s;

			RecordID curRecordID;
			int curKey;
			PageID curPageID;

			// Traverse through the index nodes, pushing their pageIDs onto a stack so we can easily
			// move back up the tree in the event of a node split (and the need to insert a new index key)
			stack<PageID> indexIDStack;
            while (curPage->GetType() == INDEX_NODE) {
				BTIndexPage *curIndexPage = (BTIndexPage *) curPage;
				curIndexID = curIndexPage->PageNo();

				indexIDStack.push(curIndexID);

				
				// While our key to insert is greater than the key of the record we are current on,
				// continue searching the index for the right path to take (get the next record)
				nextPageID = curIndexPage->GetLeftLink();
				s = curIndexPage->GetFirst(curKey,curPageID,curRecordID);
				while(s != DONE && KeyCmp(key, curKey) >= 0) {
					nextPageID = curPageID;
					s = curIndexPage->GetNext(curKey,curPageID,curRecordID);
				}

				UNPIN(curIndexID, CLEAN);

				// Pin the page of the next node we will go to
				PIN(nextPageID, curPage);
            }

			// At this point we have reached the leaf node that we wish to insert on
			BTLeafPage *curLeafPage = (BTLeafPage *) curPage;
			PageID curLeafID = curLeafPage->PageNo();

			// If there is space on the leaf node to insert the record/key do so.
			if (curLeafPage->AvailableSpace() >= GetKeyDataLength(key, LEAF_NODE)) {
				if(curLeafPage->Insert(key, rid, newRecordID) != OK) {
					UNPIN(curLeafID, CLEAN);
					return FAIL;
				}
                UNPIN(curLeafID, DIRTY);
			}
			else {
				// Since there is not enough space to insert in the leaf node. We need to split it and update the index nodes
				PageID newPageID;
                int newPageFirstKey;

				if (SplitLeafNode(key, rid, curLeafPage, newPageID, newPageFirstKey) != OK){
					UNPIN (curLeafID, CLEAN);
					return FAIL;
				}
				UNPIN(curLeafID, DIRTY);

				PageID tmpIndexID;
				BTIndexPage *tmpIndexPage;
                bool continuesplit = true;

                // Iterate through the stack containing the visited index nodes. 
				// Try to insert the key of the leftmost record of the new page created by the split
				// If necessary, split the index node and loop, now attempting to add the duplicate key
				// from the index split into the index node one level above
				int indexKey = newPageFirstKey;
                while (continuesplit) {
					// Peek at the top of the stack and pin the index page
					tmpIndexID = indexIDStack.top();
					PIN(tmpIndexID, tmpIndexPage);

					// If there is enough space in this node to insert our key, do so and terminate the loop.
					if (tmpIndexPage->AvailableSpace() >= GetKeyDataLength(indexKey, INDEX_NODE)) {
						
						if (tmpIndexPage->Insert(indexKey, newPageID, newRecordID) != OK){
							UNPIN(tmpIndexID, CLEAN);
							return FAIL;
						}
						continuesplit = false;
						UNPIN(tmpIndexID, DIRTY);
					}
					// If there is not enough space, split the index node and loop with the desired insertion key set to
					// the (now deleted) leftmost entry that was returned from splitIndexNode()
					else {
						PageID newPageID2;
						int newPageFirstKey2;
						if (SplitIndexNode(newPageFirstKey, newPageID, tmpIndexPage, newPageID2, newPageFirstKey2) != OK){
							UNPIN(tmpIndexID, CLEAN);
							return FAIL;
						}
						newPageID = newPageID2;
						indexKey = newPageFirstKey2;
						UNPIN(tmpIndexID, DIRTY);

						// Remove the processed indexNodePageID from the stack
						indexIDStack.pop();

						// If the stack is empty, create a new index node to wrap the nodes below and insert the key into
						// the newly created node. Terminate the loop after this since we are now at the root of the tree
						if (indexIDStack.empty()) {
							PageID newIndexPageID;
							Page *newPage;
                
							NEWPAGE(newIndexPageID, newPage);
				
							BTIndexPage *newIndexPage = (BTIndexPage *) newPage;
							newIndexPage->Init(newIndexPageID);
							newIndexPage->SetType(INDEX_NODE);
							newIndexPage->SetPrevPage(tmpIndexID);

							if (newIndexPage->Insert(indexKey, newPageID, newRecordID) != OK) {
								UNPIN(newIndexPageID, CLEAN);
								return FAIL;
							}

							header->SetRootPageID(newIndexPageID);
							UNPIN(newIndexPageID, DIRTY);
							continuesplit = false;
						}						
					}
                }               
			}
		}
	}
	return OK;
}


Status
BTreeFile::SplitLeafNode(const int key, const RecordID rid, BTLeafPage *fullPage, PageID &newPageID, int &newPageFirstKey)
{
	Page *newPage;
	NEWPAGE(newPageID, newPage);
	BTLeafPage *newLeafPage = (BTLeafPage *) newPage;
	newLeafPage->Init(newPageID);
	newLeafPage->SetType(LEAF_NODE);

	// If the page of the full leaf node has a valid next page, set the next page's prev-pointer to point to the new page
	PageID nextPageID = fullPage->GetNextPage();
	if (nextPageID != INVALID_PAGE) {
		Page *nextPage;
		PIN(nextPageID, nextPage);
		BTLeafPage *nextLeafPage = (BTLeafPage *) nextPage;
		nextLeafPage->SetPrevPage(newPageID);
		UNPIN(nextPageID, DIRTY);
	}

	// Link the new pages together in the B+ tree
    newLeafPage->SetNextPage(nextPageID);
	newLeafPage->SetPrevPage(fullPage->PageNo());
    fullPage->SetNextPage(newPageID);

	// Move all of the records from the old page to the new page
	while (true) {
		int movedKey;
		RecordID movedVal, firstRid, insertedRid;
		Status s = fullPage->GetFirst(movedKey,movedVal,firstRid);
		if (s == DONE) break;
		if (newLeafPage->Insert(movedKey, movedVal, insertedRid) != OK) {
			std::cerr << "Moving records failed on insert while splitting leaf node num=" << fullPage->PageNo() << std::endl;
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
		if (fullPage->DeleteRecord(firstRid) != OK) {
			std::cerr << "Moving records failed on delete while splitting leaf node num=" << fullPage->PageNo() << std::endl;
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
	}

	// Move the items back from the new page to the old page until the space is relatively equal
	// If we run across the insert point for our new value while this is occuring, insert it. This will
	// be taken care of later if it is not done during this step.
	bool didInsert = false;
	int curKey;
	RecordID curRid, curVal, insertedRid;
	newLeafPage->GetFirst(curKey,curVal,curRid);
	while (fullPage->AvailableSpace() > newLeafPage->AvailableSpace()) {

		// Check if the key we are currently on is bigger than the one we are trying to insert
		// If so, this is our insert oppertunity so insert the new value into the old (first) page
		if(!didInsert && KeyCmp(curKey,key) > 0){
			if (fullPage->Insert(key, rid, insertedRid) != OK){
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}
			didInsert = true;
		}
		else {
			if (fullPage->Insert(curKey,curVal,insertedRid) != OK){
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}
			if (newLeafPage->DeleteRecord(curRid) != OK){
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}

			newLeafPage->GetCurrent(curKey, curVal, curRid);
		}
	}

	// If we were unable to insert our new value in the previous step, do so now. However now it is inserted into the new (second) page.
	if (!didInsert) {
		if (newLeafPage->Insert(key, rid, insertedRid) != OK){
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
	}

	// Set the output which is the first key of the new (second) page
	int newKey;
	newLeafPage->GetFirst(newPageFirstKey, curVal, curRid);
	UNPIN(newPageID, DIRTY);
	return OK;
}

Status
BTreeFile::SplitIndexNode(const int key, const PageID pid, BTIndexPage *fullPage, PageID &newPageID, int &newPageFirstKey)
{
	Page *newPage;
	NEWPAGE(newPageID, newPage);
	BTIndexPage *newIndexPage = (BTIndexPage *) newPage;
	newIndexPage->Init(newPageID);
	newIndexPage->SetType(INDEX_NODE);


	// Move all the records from the old page to the new page
	while (true) {
		int movedKey;
		RecordID firstRid, insertedRid;
		PageID movedVal;
		Status s = fullPage->GetFirst(movedKey, movedVal, firstRid);
		if (s == DONE) break;
		if (newIndexPage->Insert(movedKey, movedVal, insertedRid) != OK) {
			std::cerr << "Moving records failed on insert while splitting index node num=" << fullPage->PageNo() << std::endl;
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
		if (fullPage->DeleteRecord(firstRid) != OK) {
			std::cerr << "Moving records failed on delete while splitting index node num=" << fullPage->PageNo() << std::endl;
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
	}

	// Move the items back from the new page to the old page until the space is relatively equal
	// If we run across the insert point for our new value while this is occuring, insert it. This will
	// be taken care of later if it is not done during this step.
	bool didInsert = false;
	int curKey;
	RecordID curRid, insertedRid;
	PageID curVal;
	newIndexPage->GetFirst(curKey, curVal, curRid);
	while (fullPage->AvailableSpace() > newIndexPage->AvailableSpace()) {
		// Check if the key we are currently on is bigger than the one we are trying to insert
		// If so, this is our insert oppertunity so insert the new value into the old (first) page
		if(!didInsert && KeyCmp(curKey,key) > 0){
			if (fullPage->Insert(key, pid, insertedRid) != OK){
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}
			didInsert = true;
		}
		else {
			if (fullPage->Insert(curKey,curVal,insertedRid) != OK) {
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}
			if (newIndexPage->DeleteRecord(curRid) != OK){
				UNPIN(newPageID, DIRTY);
				return FAIL;
			}
			newIndexPage->GetFirst(curKey, curVal, curRid);
		}
	}

	// If we were unable to insert our new value in the previous step, do so now. However now it is inserted into the new (second) page.
	if (!didInsert) {
		if (newIndexPage->Insert(key, pid, insertedRid) != OK){
			UNPIN(newPageID, DIRTY);
			return FAIL;
		}
	}

	// Set the output which is the first key of the new (second) page
	newIndexPage->GetFirst(newPageFirstKey, curVal, curRid);

	// Set the left link of the new index node and delete the duplicate key
	newIndexPage->SetLeftLink(curVal);
	newIndexPage->DeleteRecord(curRid);

	UNPIN(newPageID, DIRTY);

	return OK;
}

//-------------------------------------------------------------------
// BTreeFile::Delete
//
// Input   : key - the value of the key to be deleted.
//           rid - RecordID of the record to be deleted.
// Output  : None
// Return  : OK if successful, FAIL otherwise. 
// Purpose : Delete an index entry with this rid and key.  
// Note    : If the root becomes empty, delete it.
//-------------------------------------------------------------------

Status 
BTreeFile::Delete(const int key, const RecordID rid)
{
    // TODO: add your code here
    if (header->GetRootPageID() == INVALID_PAGE) return FAIL;
		
	// A root page exists
	PageID rootID = header->GetRootPageID();
	SortedPage *rootPage;
	PIN(rootID, rootPage);

	// Check if the root is a leaf node (trivial case)
	if (rootPage->GetType() == LEAF_NODE) {
		BTLeafPage *curPage = (BTLeafPage *) rootPage;
		RecordID deletedRID;
		if (curPage->Delete(key,rid,deletedRID) != OK) {
			UNPIN(rootID, CLEAN);
			return FAIL;
		}

		// If the root page is now empty we need to delete it
		if (curPage->IsEmpty()){
			FREEPAGE(rootID);
			header->SetRootPageID(INVALID_PAGE);
		}
		else {
			UNPIN (rootID, DIRTY);
		}
	}
	else{
		// If the root is not a leaf node. We need to traverse the tree to find the leaf node to delete from
        SortedPage* curPage = rootPage;
		PageID curIndexID;
		PageID nextPageID;
		Status s;

		RecordID curRecordID;
		int curKey;
		PageID curPageID;

		// For each visited index node, push it onto the stack (required from redistribution/merge extra credit - Not yet implemented)
		stack<PageID> indexIDStack;
        while (curPage->GetType() == INDEX_NODE) {
			BTIndexPage *curIndexPage = (BTIndexPage *) curPage;
			curIndexID = curIndexPage->PageNo();

			indexIDStack.push(curIndexID);

			nextPageID = curIndexPage->GetLeftLink();
			s = curIndexPage->GetFirst(curKey, curPageID, curRecordID);

			// While our key is greater than the key of the current record, continue searching the index for the right path to take
			// by getting the next record on the page
			while(s != DONE && KeyCmp(key, curKey) >= 0) {
				nextPageID = curPageID;
				s = curIndexPage->GetNext(curKey, curPageID, curRecordID);
			}
			
			UNPIN(curIndexID, CLEAN);

			// Pin the page of the next node to visit
			PIN(nextPageID, curPage);
        }

		BTLeafPage *curLeafPage = (BTLeafPage *) curPage;
		PageID curLeafID = curLeafPage->PageNo();

		// Check if this is the first key on the page. NOT NEEDEED ANYMORE, SEE PIAZZA POST @202
		RecordID curVal;
		if (curLeafPage->GetFirst(curKey, curVal, curRecordID) != OK) {
			std::cout << "GetFirst failed for pageNo" << curLeafPage->PageNo() << std::endl;
			UNPIN(curLeafID, CLEAN);
			return FAIL;
		}

		// THIS HAS BEEN REMOVED DUE TO PIAZZA POST @202
		if (false && KeyCmp(curKey, key) == 0 && curVal == rid) {
			// It is the first one
			// TODO: IMPLEMENT
			RecordID deletedRID;
			if (curLeafPage->Delete(key,rid,deletedRID) != OK) {

				UNPIN(curLeafID, CLEAN);
				return FAIL;
			}

			// If the page is now empty we need to delete it (and its entry) as long as it is not the leftmost link of an index node
			if (curPage->IsEmpty()){
				if (curLeafPage->GetPrevPage() != INVALID_PAGE){
				
				}
				else{
					// This is the leftmost page in the B+ tree, there is nothing to delete from the index
				}
				FREEPAGE(curLeafID);
			}
			else {
				UNPIN (rootID, DIRTY);
			}
		}
		else {
			// Simply delete the entry from the leaf page
			RecordID deleteRID;
			if (curLeafPage->Delete(key,rid,deleteRID) != OK) {
				std::cout << "Delete failed for thing with key= " << key << std::endl;
				UNPIN(curLeafID, CLEAN);
				return FAIL;
			}
			UNPIN (curLeafID, DIRTY);
		}
	}

	return OK;
}


//-------------------------------------------------------------------
// BTreeFile::OpenScan
//
// Input   : lowKey, highKey - pointer to keys, indicate the range
//                             to scan.
// Output  : None
// Return  : A pointer to IndexFileScan class.
// Purpose : Initialize a scan.  
// Note    : Usage of lowKey and highKey :
//
//           lowKey      highKey      range
//			 value	     value	
//           --------------------------------------------------
//           nullptr     nullptr      whole index
//           nullptr     !nullptr     minimum to highKey
//           !nullptr    nullptr      lowKey to maximum
//           !nullptr    =lowKey  	  exact match
//           !nullptr    >lowKey      lowKey to highKey
//-------------------------------------------------------------------

IndexFileScan*
BTreeFile::OpenScan(const int* lowKey, const int* highKey)
{
    // TODO: add your code here
	BTreeFileScan *newScan = new BTreeFileScan();

	const  int* searchKey = NULL;
	if (lowKey != NULL) {
		searchKey = lowKey;
	}
	else
	{
		int nagtive = -1;
		searchKey = &nagtive;
	}
	
	PageID leftmostPageID;
	if (Search(searchKey,leftmostPageID) != OK) {
		leftmostPageID = INVALID_PAGE;
	}
	newScan->Init(lowKey, highKey, leftmostPageID);

	return newScan;
}


//-------------------------------------------------------------------
// BTreeFile::PrintTree
//
// Input   : pageID - root of the tree to print.
// Output  : None
// Return  : None
// Purpose : Print out the content of the tree rooted at pid.
//-------------------------------------------------------------------

Status 
BTreeFile::PrintTree(PageID pageID)
{ 
	if ( pageID == INVALID_PAGE ) {
    	return FAIL;
	}

	SortedPage* page = nullptr;
	PIN(pageID, page);
	
	NodeType type = (NodeType) page->GetType();
	if (type == INDEX_NODE)
	{
		BTIndexPage* index = (BTIndexPage *) page;
		PageID curPageID = index->GetLeftLink();
		PrintTree(curPageID);

		RecordID curRid;
		int key;		
		Status s = index->GetFirst(key, curPageID, curRid);
		while (s != DONE)
		{
			PrintTree(curPageID);
			s = index->GetNext(key, curPageID, curRid);
		}
	}

	UNPIN(pageID, CLEAN);
	PrintNode(pageID);

	return OK;
}

//-------------------------------------------------------------------
// BTreeFile::PrintNode
//
// Input   : pageID - the node to print.
// Output  : None
// Return  : None
// Purpose : Print out the content of the node pid.
//-------------------------------------------------------------------

Status 
BTreeFile::PrintNode(PageID pageID)
{	
	SortedPage* page = nullptr;
	PIN(pageID, page);

	NodeType type = (NodeType) page->GetType();
	switch (type)
	{
		case INDEX_NODE:
		{
			BTIndexPage* index = (BTIndexPage *) page;
			PageID curPageID = index->GetLeftLink();
			cout << "\n---------------- Content of index node " << pageID << "-----------------------------" << endl;
			cout << "\n Left most PageID:  "  << curPageID << endl;

			RecordID currRid;
			int key, i = 0;

			Status s = index->GetFirst(key, curPageID, currRid); 
			while (s != DONE)
			{	
				i++;
				cout <<  "Key: " << key << "	PageID: " << curPageID << endl;
				s = index->GetNext(key, curPageID, currRid);
			}
			cout << "\n This page contains  " << i << "  entries." << endl;
			break;
		}

		case LEAF_NODE:
		{
			BTLeafPage* leaf = (BTLeafPage *) page;
			cout << "\n---------------- Content of leaf node " << pageID << "-----------------------------" << endl;

			RecordID dataRid, currRid;
			int key, i = 0;

			Status s = leaf->GetFirst(key, dataRid, currRid);
			while (s != DONE)
			{	
				i++;
				cout << "DataRecord ID: " << dataRid << " Key: " << key << endl;
				s = leaf->GetNext(key, dataRid, currRid);
			}
			cout << "\n This page contains  " << i << "  entries." << endl;
			break;
		}
	}
	UNPIN(pageID, CLEAN);

	return OK;
}

//-------------------------------------------------------------------
// BTreeFile::Print
//
// Input   : None
// Output  : None
// Return  : None
// Purpose : Print out this B+ Tree
//-------------------------------------------------------------------

Status 
BTreeFile::Print()
{	
	cout << "\n\n-------------- Now Begin Printing a new whole B+ Tree -----------" << endl;


	if (PrintTree(header->GetRootPageID()) == OK)
		return OK;

	return FAIL;
}

//-------------------------------------------------------------------
// BTreeFile::DumpStatistics
//
// Input   : None
// Output  : None
// Return  : None
// Purpose : Print out the following statistics.
//           1. Total number of leaf nodes, and index nodes.
//           2. Total number of leaf entries.
//           3. Total number of index entries.
//           4. Mean, Min, and max fill factor of leaf nodes and 
//              index nodes.
//           5. Height of the tree.
//-------------------------------------------------------------------
Status 
BTreeFile::DumpStatistics()
{	
	// TODO: add your code here	
	ostream& os = std::cout;
	float avgDataFillFactor, avgIndexFillFactor;

// initialization 
	hight = totalDataPages = totalIndexPages = totalNumIndex = totalNumData = 0;
	maxDataFillFactor = maxIndexFillFactor = 0; minDataFillFactor = minIndexFillFactor =1;
	totalFillData = totalFillIndex = 0;

	if(_DumpStatistics(header->GetRootPageID())== OK)
	{		// output result
		if (totalNumData == 0)
			maxDataFillFactor = minDataFillFactor = avgDataFillFactor = 0;
		else
			avgDataFillFactor = totalFillData/totalDataPages;
		if ( totalNumIndex == 0)
			maxIndexFillFactor = minIndexFillFactor = avgIndexFillFactor = 0;
		else 
			avgIndexFillFactor = totalFillIndex/totalIndexPages;
		os << "\n------------ Now dumping statistics of current B+ Tree!---------------" << endl;
		os << "  Total nodes are        : " << totalDataPages + totalIndexPages << " ( " << totalDataPages << " Data";
		os << "  , " << totalIndexPages <<" indexpages )" << endl;
		os << "  Total data entries are : " << totalNumData << endl;
		os << "  Total index entries are: " << totalNumIndex << endl;
		os << "  Hight of the tree is   : " << hight << endl;
		os << "  Average fill factors for leaf is : " << avgDataFillFactor<< endl;
		os << "  Maximum fill factors for leaf is : " << maxDataFillFactor<<endl;;
		os << "	  Minumum fill factors for leaf is : " << minDataFillFactor << endl;
		os << "  Average fill factors for index is : " << 	avgIndexFillFactor << endl;
		os << "  Maximum fill factors for index is : " << maxIndexFillFactor<<endl;;
		os << "	  Minumum fill factors for index is : " << minIndexFillFactor << endl;
		os << "  That's the end of dumping statistics." << endl;

		return OK;
	}
	return FAIL;
}

Status
BTreeFile::_DumpStatistics(PageID pageID)
{
	DumpHelper(pageID);

	SortedPage *page;
	BTIndexPage *index;

	Status s;
	PageID curPageID;

	RecordID curRid;
	int key;

	PIN (pageID, page);
	NodeType type = (NodeType)page->GetType();

	switch (type) {
	case INDEX_NODE:
		index = (BTIndexPage *)page;
		curPageID = index->GetLeftLink();
		_DumpStatistics(curPageID);
		s=index->GetFirst(key, curPageID,curRid);
		if ( s == OK) {	
			_DumpStatistics(curPageID);
			s = index->GetNext(key, curPageID,curRid);
			while ( s != DONE) {	
				_DumpStatistics(curPageID);
				s = index->GetNext(key, curPageID,curRid);
			}
		}
		UNPIN(pageID, CLEAN);
		break;

	case LEAF_NODE:
		UNPIN(pageID, CLEAN);
		break;
	default:		
		assert (0);
	}

	return OK;
}

Status
BTreeFile::DumpHelper (PageID pageID)
{
	SortedPage *page;
	BTIndexPage *index;
	BTLeafPage *leaf;
	int i;
	Status s;
	PageID curPageID;
	float	curFillFactor;
	RecordID curRid;

	int  key;
	RecordID dataRid;

	PIN (pageID, page);
	NodeType type = (NodeType)page->GetType ();
	i = 0;
	switch (type) {
	case INDEX_NODE:
		// add totalIndexPages
		totalIndexPages++;
		if ( hight <= 0) // still not reach the bottom
			hight--;
		index = (BTIndexPage *)page;
		curPageID = index->GetLeftLink();
		s=index->GetFirst (key, curPageID, curRid); 
		if ( s == OK) {	
			i++;
			s = index->GetNext(key, curPageID, curRid);
			while (s != DONE) {	
				i++;
				s = index->GetNext(key, curPageID, curRid);
			}
		}
		totalNumIndex  += i;
		curFillFactor = (float)(1.0 - 1.0*(index->AvailableSpace())/MAX_SPACE);
		if ( maxIndexFillFactor < curFillFactor)
			maxIndexFillFactor = curFillFactor;
		if ( minIndexFillFactor > curFillFactor)
			minIndexFillFactor = curFillFactor;
		totalFillIndex += curFillFactor;
		UNPIN(pageID, CLEAN);
		break;

	case LEAF_NODE:
		// when the first time reach a leaf, set hight
		if ( hight < 0)
			hight = -hight;
		totalDataPages++;

		leaf = (BTLeafPage *)page;
		s = leaf->GetFirst (key, dataRid, curRid);
		if (s == OK) {	
			s = leaf->GetNext(key, dataRid, curRid);
			i++;
			while ( s != DONE) {	
				i++;	
				s = leaf->GetNext(key, dataRid, curRid);
			}
		}
		totalNumData += i;
		curFillFactor = (float)(1.0 - 1.0*leaf->AvailableSpace()/MAX_SPACE);
		if ( maxDataFillFactor < curFillFactor)
			maxDataFillFactor = curFillFactor;
		if ( minDataFillFactor > curFillFactor)
			minDataFillFactor = curFillFactor;
		totalFillData += curFillFactor;
		UNPIN(pageID, CLEAN);
		break;
	default:		
		assert (0);
	}

	return OK;
}

int
BTreeFile::GetKeyDataLength(const int key, const NodeType nodeType){
	if(nodeType == INDEX_NODE)
	{
		IndexEntry node;
		return sizeof(IndexEntry);
	}
	else
	{
		LeafEntry node;
		return sizeof(LeafEntry);
	}
}

int
BTreeFile::KeyCmp(const int key1, const int key2){
	if (key1 < key2)
	{
		return -1;
	}
	else if (key1 == key2)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

Status BTreeFile:: Search(const int *key,  PageID& foundPid)
{
	if (header->GetRootPageID() == INVALID_PAGE)
	{
		foundPid = INVALID_PAGE;
		return DONE;
		
	}

	Status s;
	
	//cout<<"try to run _Search function"<<endl;
	s = _Search(key, header->GetRootPageID(), foundPid);
	if (s != OK)
	{
		cerr << "Search FAIL in BTreeFile::Search\n";
		return FAIL;
	}

	return OK;
}

Status
BTreeFile::_Search( const int *key,  PageID currID, PageID& foundID)
{
	
    SortedPage *page;
	Status s;
	
    PIN (currID, page);
    NodeType type = (NodeType)page->GetType ();
	cout<<"_Search:finish pin"<<endl;
	
    switch (type) 
	{
	case INDEX_NODE:
		cout<<"_Search:it is an index node"<<endl;
		s =	_SearchIndex(key,  currID, (BTIndexPage*)page, foundID);
		break;
		
	case LEAF_NODE:
		cout<<"_Search:it is a leaf node"<<endl;
		foundID =  page->PageNo();
		UNPIN(currID,CLEAN);
		break;
	default:		
		assert (0);
	}
	return OK;
}


Status BTreeFile::_SearchIndex (const int *key,  PageID currIndexID, BTIndexPage *currIndex, PageID& foundID)
{
	PageID nextPageID;
	cout<<"try to get pageID"<<endl;
	Status s = currIndex->GetPageID(key, nextPageID);
	cout<<"nextPageID:"<<nextPageID<<endl;
	if (s != OK)
	{
		return FAIL;
	}
		
	// Now unpin the page, recurse and then pin it again
	cout<<"_SearchIndex:start unpin the page"<<endl;
	UNPIN (currIndexID, CLEAN);
	cout<<"_SearchIndex:finish unpin"<<endl;
	s = _Search (key, nextPageID, foundID);
	if (s != OK)
		return FAIL;
	return OK;
}


Status 
BTreeFile::PrintTree2 ( PageID pageID, int option)
{ 
	_PrintTree(pageID);
	if (option == 0) return OK;

	SortedPage *page;
	BTIndexPage *index;

	Status s;
	PageID curPageID;

	RecordID curRid;
	int  key;

    PIN (pageID, page);
    NodeType type = (NodeType)page->GetType ();
	
	switch (type) {
	case INDEX_NODE:
		index = (BTIndexPage *)page;
		curPageID = index->GetLeftLink();
		PrintTree2(curPageID, 1);
		s=index->GetFirst (key, curPageID, curRid);
		if ( s == OK)
		{	
			PrintTree2(curPageID, 1);
			s = index->GetNext(key, curPageID, curRid);
			while ( s != DONE)
			{	
				PrintTree2(curPageID, 1);
				s = index->GetNext(key, curPageID, curRid);
			}
		}
		UNPIN(pageID, CLEAN);
		break;

	case LEAF_NODE:
		UNPIN(pageID, CLEAN);
		break;
	default:		
		assert (0);
	}

	return OK;
}

Status 
BTreeFile::_PrintTree ( PageID pageID)
{
	SortedPage *page;
	BTIndexPage *index;
	BTLeafPage *leaf;
	int i;
	Status s;
	PageID curPageID;

	RecordID curRid;

	int  key;
	RecordID dataRid;

	ostream& os = cout;

    PIN (pageID, page);
    NodeType type = (NodeType)page->GetType();
	i = 0;
      switch (type) 
	{
	case INDEX_NODE:
			index = (BTIndexPage *)page;
			curPageID = index->GetLeftLink();
			os << "\n---------------- Content of Index_Node-----   " << pageID <<endl;
			os << "\n Left most PageID:  "  << curPageID << endl;
			s=index->GetFirst (key, curPageID, curRid); 
			if ( s == OK)
			{	i++;
				os <<"Key: "<< key<< "	PageID: " 
					<< curPageID  << endl;
				s = index->GetNext(key, curPageID, curRid);
				while ( s != DONE)
				{	
					os <<"Key: "<< key<< "	PageID: " 
						<< curPageID  << endl;
					i++;
					s = index->GetNext(key, curPageID, curRid);

				}
			}
			os << "\n This page contains  " << i <<"  Entries!" << endl;
			UNPIN(pageID, CLEAN);
			break;
		
	case LEAF_NODE:
		leaf = (BTLeafPage *)page;
		s = leaf->GetFirst (key, dataRid, curRid);
			if ( s == OK)
			{	os << "\n Content of Leaf_Node"  << pageID << endl;
				os <<   "Key: "<< key<< "	DataRecordID: " 
					<< dataRid  << endl;
				s = leaf->GetNext(key, dataRid, curRid);
				i++;
				while ( s != DONE)
				{	
					os <<   "Key: "<< key<< "	DataRecordID: " 
						<< dataRid  << endl;
					i++;	
					s = leaf->GetNext(key, dataRid, curRid);
				}
			}
			os << "\n This page contains  " << i <<"  entries!" << endl;
			UNPIN(pageID, CLEAN);
			break;
	default:		
		assert (0);
	}
	//os.close();

	return OK;	
}

Status
BTreeFile::Delete1(const int key, const RecordID rid)
{
	SortedPage* rootPage; 
	PageID rootPid;
	Status s = OK;
	Status res;
	NodeType type;
	int key2; RecordID rid2; RecordID keyRecordID;

	rootPid = header->GetRootPageID();
	PIN(rootPid, (Page *&)rootPage);

 	type = (NodeType)rootPage->GetType();
	if (type == LEAF_NODE) {
		BTLeafPage *leafPage = (BTLeafPage *)rootPage;
		RecordID deletedRID;
		res = leafPage->Delete(key, rid,deletedRID);

		if (leafPage->GetNumOfRecords() == 0) {
			header->SetRootPageID(INVALID_PAGE);
		}

		UNPIN(rootPid,DIRTY);
		return res;
	} else {
		PageID childPid;
		BTIndexPage *indexPage =(BTIndexPage *)rootPage;
		
		PageID oldPid;
		bool rightSibling;
		s = indexPage->GetPageID(&key, childPid);
		res = _Delete(rootPid, childPid, key, rid, oldPid, rightSibling);

		if (res == FAIL) {
			UNPIN(rootPid,DIRTY);
			return FAIL;
		}

		if (oldPid != INVALID_PAGE) {
			indexPage->DeletePage(oldPid, rightSibling);
			PageID firstPid;
			key2 = -1;
			s = indexPage->GetFirst(key2, firstPid, rid2);
			if (s == DONE) {
				firstPid = indexPage->GetLeftLink();
				header->SetRootPageID(firstPid);
			}
			key2 = NULL;
		}
		UNPIN(rootPid,DIRTY);
		return res;
	}
}

Status
BTreeFile::_Delete(PageID parentPid, PageID nodePid, int key, const RecordID rid, PageID& oldPid, bool& rightSibling)
{
	RecordID tempRid;
	int tempKey = 0;
	Status res;
	Status s;

	BTIndexPage *parentPage;
	PIN(parentPid, (Page *&)parentPage);
	
	SortedPage *nodePage;
	PIN(nodePid, (Page *&)nodePage);
	NodeType nodeType = (NodeType)nodePage->GetType();

	if (nodeType == LEAF_NODE) {
		BTLeafPage * nodePageL = (BTLeafPage *) nodePage;
		RecordID deletedRID;
		res = nodePageL->Delete(key, rid,deletedRID);

		if (res == FAIL || nodePageL->AvailableSpace() <= HEAPPAGE_DATA_SIZE/2) {
			oldPid = INVALID_PAGE;
			UNPIN(parentPid, DIRTY);
			UNPIN(nodePid, DIRTY);
			return res;
		}

		PageID siblingPid;
		parentPage->FindSiblingForChild(nodePid, siblingPid, rightSibling);

		BTLeafPage *siblingPage;
		PIN(siblingPid, (Page *&)siblingPage);

		int oldParentKey = 0;
		RecordID tempDrid;
		if (rightSibling) {
			s = siblingPage->GetFirst(tempKey, tempDrid, tempRid);
			s = parentPage->FindKey(tempKey, oldParentKey);
		} else {
			s = nodePageL->GetFirst(tempKey, tempDrid, tempRid);
			parentPage->FindKey(tempKey, oldParentKey);
		}

		// redistribute
		while(nodePageL->AvailableSpace() > HEAPPAGE_DATA_SIZE/2) {
			if (rightSibling) {
				s = siblingPage->GetFirst(tempKey, tempDrid, tempRid);
				s = nodePageL->Insert(tempKey, tempDrid, tempRid);
				RecordID deletedRID;
				if(s == OK) siblingPage->Delete(tempKey, tempDrid,deletedRID);
			} else {
				s = siblingPage->GetLast(tempRid,tempKey,tempDrid);
				s= nodePageL->Insert(tempKey, tempDrid, tempRid);
				if(s == OK) {
					RecordID deletedRID;
					siblingPage->Delete(tempKey, tempDrid, deletedRID);
				}
			}
		}

		// redistribution successful
		if (siblingPage->AvailableSpace() <= HEAPPAGE_DATA_SIZE/2) {
			if (rightSibling) {
				s = siblingPage->GetFirst(tempKey, tempDrid,tempRid);
			} else {
				s = nodePageL->GetFirst(tempKey, tempDrid, tempRid);
			}
			parentPage->AdjustKey(tempKey, oldParentKey);
			oldPid = INVALID_PAGE;
			UNPIN(parentPid, DIRTY);
			UNPIN(nodePid, DIRTY);
			UNPIN(siblingPid, DIRTY);
			return res;
		} else {
			if (siblingPage->AvailableSpace() + nodePageL->AvailableSpace() >= HEAPPAGE_DATA_SIZE) {
				// merge
				while (true) {
					s = siblingPage->GetFirst(tempKey, tempDrid, tempRid);
					if(s == OK) {
						s = nodePageL->Insert(tempKey, tempDrid, tempRid);
						RecordID deletedRID;
						siblingPage->Delete(tempKey, tempDrid,deletedRID);
					} else if (s == DONE) {
						break;
					};
				}
				if (rightSibling) {
					PageID nnPid = siblingPage->GetNextPage();
					if (nnPid != INVALID_PAGE) {
						BTLeafPage *nnPage;
						PIN(nnPid, (Page *&)nnPage);
						nnPage->SetPrevPage(nodePid);
						UNPIN(nnPid, DIRTY);
					}
					nodePageL->SetNextPage(nnPid);
				} else {
					PageID ppPid = siblingPage->GetPrevPage();
					if (ppPid != INVALID_PAGE) {
						BTLeafPage *ppPage;
						PIN(ppPid, (Page *&)ppPage);
						ppPage->SetNextPage(nodePid);
						UNPIN(ppPid, DIRTY);
					}
					nodePageL->SetPrevPage(ppPid);
				}
				oldPid = siblingPid;
				UNPIN(parentPid, DIRTY);
				UNPIN(nodePid, DIRTY);
				UNPIN(siblingPid, DIRTY);

				return res;
			} else {
				oldPid = INVALID_PAGE;
				UNPIN(parentPid, DIRTY);
				UNPIN(nodePid, DIRTY);
				UNPIN(siblingPid, DIRTY);
				return res;
			}
		}
	} else {
		BTIndexPage * nodePageI = (BTIndexPage *) nodePage;

		PageID targetPid;
		bool leftMost;
		s = nodePageI->FindPage(key, targetPid, leftMost);

		PageID tempOldPid;
		bool tempRightSibling;

		res = _Delete(nodePid, targetPid, key, rid, tempOldPid, tempRightSibling);

		if (res == FAIL || tempOldPid == INVALID_PAGE) {
			oldPid = INVALID_PAGE;
			UNPIN(parentPid, CLEAN);
			UNPIN(nodePid, CLEAN);
			return res;
		}

		nodePageI->DeletePage(tempOldPid, tempRightSibling);

		if (nodePageI->AvailableSpace() <= HEAPPAGE_DATA_SIZE/2) {
			oldPid = INVALID_PAGE;
			UNPIN(parentPid, CLEAN);
			UNPIN(nodePid, DIRTY);
			return res;
		}

		PageID siblingPid;
		bool rightSibling;
		parentPage->FindSiblingForChild(nodePid, siblingPid, rightSibling);

		BTIndexPage *siblingPage;
		PIN(siblingPid, (Page *&)siblingPage);

		int keyToAdjust = 0;
		if (rightSibling) {
			parentPage->FindKeyWithPage(siblingPid, keyToAdjust, leftMost);
		} else {
			parentPage->FindKeyWithPage(nodePid, keyToAdjust, leftMost);
		}

		// redistribute
		PageID tempPid;
		while(nodePageI->AvailableSpace() > HEAPPAGE_DATA_SIZE/2) {
			if (rightSibling) {
				siblingPage->GetFirst(tempKey, tempPid, tempRid);
				nodePageI->Insert(keyToAdjust, siblingPage->GetLeftLink(), tempRid);
				parentPage->AdjustKey(tempKey, keyToAdjust);
				keyToAdjust = tempKey;
				siblingPage->SetLeftLink(tempPid);
				siblingPage->Delete(tempKey, tempRid);
			} else {
				s = siblingPage->GetLast(tempRid, tempKey, tempPid);
				nodePageI->Insert(keyToAdjust, nodePageI->GetLeftLink(), tempRid);
				parentPage->AdjustKey(tempKey, keyToAdjust);
				keyToAdjust = tempKey;
				nodePageI->SetLeftLink(tempPid);
				siblingPage->Delete(tempKey, tempRid);
			}
		}

		// redistribution successful
		if (siblingPage->AvailableSpace() <= HEAPPAGE_DATA_SIZE/2) {
			oldPid = INVALID_PAGE;
			UNPIN(parentPid, DIRTY);
			UNPIN(nodePid, DIRTY);
			UNPIN(siblingPid, DIRTY);
			return res;
		} else {
			if (siblingPage->AvailableSpace() + nodePageI->AvailableSpace() >= HEAPPAGE_DATA_SIZE) {
				// merge
				while (true) {
					if (rightSibling) {
						s = siblingPage->GetFirst(tempKey, tempPid, tempRid);
						nodePageI->Insert(keyToAdjust, siblingPage->GetLeftLink(), tempRid);
						if (s == DONE) {
							break;
						}
						parentPage->AdjustKey(tempKey, keyToAdjust);
						keyToAdjust = tempKey;
						siblingPage->SetLeftLink(tempPid);
						siblingPage->Delete(tempKey, tempRid);
					} else {
						
						nodePageI->Insert(keyToAdjust, nodePageI->GetLeftLink(), tempRid);
						s = siblingPage->GetLast(tempRid, tempKey, tempPid);
						if (s == DONE) {
							nodePageI->SetLeftLink(siblingPage->GetLeftLink());
							break;
						} else {
							parentPage->AdjustKey(tempKey, keyToAdjust);
							keyToAdjust = tempKey;
							nodePageI->SetLeftLink(tempPid);
							siblingPage->Delete(tempKey, tempRid);
						}
					}
				}

				s = siblingPage->GetFirst(tempKey, tempPid, tempRid);
				if (s == DONE) {
					oldPid = siblingPid;
					UNPIN(parentPid, DIRTY);
					UNPIN(nodePid, DIRTY);
					UNPIN(siblingPid, DIRTY);
					return res;
				} else {
					oldPid = INVALID_PAGE;
					UNPIN(parentPid, DIRTY);
					UNPIN(nodePid, DIRTY);
					UNPIN(siblingPid, DIRTY);
					return res;
				}
			} else {
				oldPid = INVALID_PAGE;
				UNPIN(parentPid, DIRTY);
				UNPIN(nodePid, DIRTY);
				UNPIN(siblingPid, DIRTY);
				return res;
			}
		}
	}
}

Status
BTreeFile::Delete2(const int key, const RecordID rid)
{
	if (header->GetRootPageID() == INVALID_PAGE) return FAIL;
		
	// A root page exists
	PageID rootID = header->GetRootPageID();
	SortedPage *rootPage;
	PIN(rootID, rootPage);

	// Check if the root is a leaf node (trivial case)
	if (rootPage->GetType() == LEAF_NODE) {
		BTLeafPage *curPage = (BTLeafPage *) rootPage;
		RecordID deletedRID;
		if (curPage->Delete(key,rid,deletedRID) != OK) {
			UNPIN(rootID, CLEAN);
			return FAIL;
		}

		// If the root page is now empty we need to delete it
		if (curPage->IsEmpty()){
			FREEPAGE(rootID);
			header->SetRootPageID(INVALID_PAGE);
		}
		else {
			UNPIN (rootID, DIRTY);
		}
	}
	else{
		// If the root is not a leaf node. We need to traverse the tree to find the leaf node to delete from
        SortedPage* curPage = rootPage;
		PageID curIndexID;
		PageID nextPageID;
		Status s;

		RecordID curRecordID;
		int curKey;
		PageID curPageID;

	// 	PageID leftmostPageID;
	// 	const  int* searchKey = &key;
	// 	if (Search(searchKey,leftmostPageID) != OK) {
	// 		leftmostPageID = INVALID_PAGE;
	// 		cout<<"The key does not exist"<<endl;
	// 		return FAIL;
	// 	}
	// 	cout<<"target leaf node gets"<<endl;
	// 	PIN(leftmostPageID,curPage);
	// 	RecordID deletedRID;
	// 	((BTLeafPage*)curPage)->Delete(key,rid,deletedRID);
	// 	bool furtherOperation = ((BTLeafPage*)curPage)->IsAtLeastHalfFull();
	// 	if (!furtherOperation)
	// 	{
	// 		return OK;
	// 	}

	// 	UNPIN(leftmostPageID,CLEAN);
	// 	return OK;
	// }
		// For each visited index node, push it onto the stack (required from redistribution/merge extra credit - Not yet implemented)
		stack<PageID> indexIDStack;
        while (curPage->GetType() == INDEX_NODE) {
			BTIndexPage *curIndexPage = (BTIndexPage *) curPage;
			curIndexID = curIndexPage->PageNo();

			indexIDStack.push(curIndexID);

			nextPageID = curIndexPage->GetLeftLink();
			s = curIndexPage->GetFirst(curKey, curPageID, curRecordID);

			// While our key is greater than the key of the current record, continue searching the index for the right path to take
			// by getting the next record on the page
			while(s != DONE && KeyCmp(key, curKey) >= 0) {
				nextPageID = curPageID;
				s = curIndexPage->GetNext(curKey, curPageID, curRecordID);
			}
			
			UNPIN(curIndexID, CLEAN);

			// Pin the page of the next node to visit
			PIN(nextPageID, curPage);
        }

		BTLeafPage *curLeafPage = (BTLeafPage *) curPage;
		PageID curLeafID = curLeafPage->PageNo();
		cout<<curLeafID<<endl;

		// Check if this is the first key on the page. NOT NEEDEED ANYMORE, SEE PIAZZA POST @202
		RecordID curVal;
		if (curLeafPage->GetFirst(curKey, curVal, curRecordID) != OK) {
			std::cout << "GetFirst failed for pageNo" << curLeafPage->PageNo() << std::endl;
			UNPIN(curLeafID, CLEAN);
			return FAIL;
		}

		// Simply delete the entry from the leaf page
		RecordID deleteRID;
		if (curLeafPage->Delete(key,rid,deleteRID) != OK) {
			std::cout << "Delete failed for thing with key= " << key << std::endl;
			UNPIN(curLeafID, CLEAN);
			return FAIL;
		}
		//check the page is halffull or not.
		if (!curLeafPage->IsAtLeastHalfFull())
		{
			PageID parentPageID = indexIDStack.top();
			BTIndexPage* parentPage;
			PIN(parentPageID,parentPage);
			//To find the sibling page
			PageID siblingPID;
			bool rightSibling;
			parentPage->FindSiblingForChild(curLeafID, siblingPID,rightSibling);
			BTLeafPage* siblingPage;
			PIN(siblingPID,siblingPage);
			if (siblingPage->CanBorrow()) // sibling node has enough <K,V> pair to borrow
			{
				int key = 0;
				RecordID dataRID;
				RecordID rid;
				int borrowedKey;
				RecordID borrowedDataID;
				RecordID borrowedRID;
				if (rightSibling) // borrow element from right sibling
				{
					siblingPage->GetFirst(borrowedKey,borrowedDataID,borrowedRID);
				}
				else             // borrow element from left sibling
				{
					siblingPage->GetLast(borrowedRID,borrowedKey,borrowedDataID);
				}
				curLeafPage->Insert(borrowedKey,borrowedDataID,borrowedRID);
				siblingPage->Delete(borrowedKey,borrowedDataID,borrowedRID);
				UNPIN(parentPageID,DIRTY);
				UNPIN(siblingPID,DIRTY);	
			}
			else
			{
				/* code */
			}
			
		}
		
		UNPIN (curLeafID, DIRTY);
	}

	return OK;
}
