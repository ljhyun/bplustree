 /*
 * Copyright (C) 2008 by The Regents of the University of California
 * Redistribution of this file is permitted under the terms of the GNU
 * Public License (GPL).
 *
 * @author Junghoo "John" Cho <cho AT cs.ucla.edu>
 * @date 3/24/2008
 */
 
#include "BTreeIndex.h"
#include "BTreeNode.h"
#include <iostream> 
#include <string.h>

using namespace std;

/*
 * BTreeIndex constructor
 */
BTreeIndex::BTreeIndex()
{
    rootPid = -1;
    treeHeight = 1;
}

/*
 * Open the index file in read or write mode.
 * Under 'w' mode, the index file should be created if it does not exist.
 * @param indexname[IN] the name of the index file
 * @param mode[IN] 'r' for read, 'w' for write
 * @return error code. 0 if no error
 */
RC BTreeIndex::open(const string& indexname, char mode)
{
    RC rc = 0;
    int flag;
    PageFile page(indexname,mode);
    pf = page;
    //rc = pf.open(indexname,mode);
    char buffer[pf.PAGE_SIZE];
    rc = pf.read(0,buffer);
    memcpy(&flag,buffer,sizeof(int));
    
    if (flag==-1) {
        memcpy(&rootPid,buffer+sizeof(int),sizeof(int));
        memcpy(&treeHeight,buffer+sizeof(int)+sizeof(int),sizeof(int));
    }
    return rc;
}

/*
 * Close the index file.
 * @return error code. 0 if no error
 */
RC BTreeIndex::close()
{
    RC rc = 0;
    int flag = -1;
    char buffer[pf.PAGE_SIZE];
    memcpy(buffer,&flag,sizeof(int));
    memcpy(buffer+sizeof(int),&rootPid,sizeof(int));
    memcpy(buffer+sizeof(int)+sizeof(int),&treeHeight,sizeof(int));
    rc = pf.write(0,&buffer);
    pf.close();
    return rc;
}

/*
 * Insert (key, RecordId) pair to the index.
 * @param key[IN] the key for the value inserted into the index
 * @param rid[IN] the RecordId for the record being inserted into the index
 * @return error code. 0 if no error
 */
RC BTreeIndex::insert(int key, const RecordId& rid)
{
    int rc = 0;
    if (rootPid==-1) {
        rootPid = 1;
        BTLeafNode init;
        init.insert(key,rid);
        init.setNextNodePtr(-1);
        init.write(1,pf);
    }
    else if (treeHeight == 1) {
        BTLeafNode temproot;
        temproot.read(1,pf);      
        if ((rc=temproot.insert(key,rid))==RC_NODE_FULL) {
            BTNonLeafNode root;
            BTLeafNode right;
            int rightkey;
            rc = temproot.insertAndSplit(key,rid,right,rightkey);
            int mid = temproot.getKey();
            root.initializeRoot(2,mid,3);
            temproot.setNextNodePtr(3);
            right.setNextNodePtr(-1);
            root.write(1,pf);
            temproot.write(2,pf);
            right.write(3,pf);
            treeHeight=2;
            return rc;
        }
        else {
            temproot.write(1,pf);
            return 0;
        }
    }
    else {
       return insertHelper(key,rid,1,rootPid);
    }

    return 0;
}

RC BTreeIndex::insertHelper(int& key, const RecordId& rid, int height, PageId& pid) {
    RC rc=0;

    if (height == treeHeight) {
        BTLeafNode ins;
        ins.read(pid,pf);
        if ((rc=ins.insert(key,rid))==RC_NODE_FULL) {
            BTLeafNode right;
            int rightkey;
            PageId newPid = pf.endPid();
            rc = ins.insertAndSplit(key,rid,right,rightkey);
            PageId tempid = ins.getNextNodePtr();
            ins.setNextNodePtr(newPid);
            right.setNextNodePtr(tempid);
            ins.write(pid,pf);
            right.write(newPid,pf);
            key = rightkey;
            pid = newPid;
            return (rc<0) ? rc:-1;        
        }
        else {
            ins.write(pid,pf);
            return 0;
        }
    }
    else if (treeHeight > height) {
        RC rc=0;
        PageId nextChild;
        BTNonLeafNode ref;
        ref.read(pid,pf);
        ref.locateChildPtr(key,nextChild);
        int nFilled = insertHelper(key,rid,height+1,nextChild);
        if (nFilled == -1 ) {
            if((rc=ref.insert(key,nextChild))==RC_NODE_FULL) {
                BTNonLeafNode right;
                int mid;
                PageId newPid = pf.endPid();
                rc = ref.insertAndSplit(key,nextChild,right,mid);
                ref.write(pid,pf);
                right.write(newPid,pf);
                if (height==1) {
                    BTNonLeafNode root;
                    root.initializeRoot(pid,mid,newPid);
                    rootPid = pf.endPid();
                    root.write(rootPid,pf);
                    treeHeight++;
                    return (rc<0) ? rc:0;
                }
                else {
                    key = mid;
                    pid = newPid;
                    return (rc<0) ? rc:-1;
                }
            }
            else {
                ref.write(pid,pf);
                return 0;
            }
        } 
        else {
            return nFilled;
        } 
   
    }
    else {
        return RC_INVALID_ATTRIBUTE;
    }

}
/**
 * Run the standard B+Tree key search algorithm and identify the
 * leaf node where searchKey may exist. If an index entry with
 * searchKey exists in the leaf node, set IndexCursor to its location
 * (i.e., IndexCursor.pid = PageId of the leaf node, and
 * IndexCursor.eid = the searchKey index entry number.) and return 0.
 * If not, set IndexCursor.pid = PageId of the leaf node and
 * IndexCursor.eid = the index entry immediately after the largest
 * index key that is smaller than searchKey, and return the error
 * code RC_NO_SUCH_RECORD.
 * Using the returned "IndexCursor", you will have to call readForward()
 * to retrieve the actual (key, rid) pair from the index.
 * @param key[IN] the key to find
 * @param cursor[OUT] the cursor pointing to the index entry with
 *                    searchKey or immediately behind the largest key
 *                    smaller than searchKey.
 * @return 0 if searchKey is found. Othewise an error code
 */
RC BTreeIndex::locate(int searchKey, IndexCursor& cursor) {
    RC rc;
    BTNonLeafNode nr;
    PageId cpid = rootPid;
    for (int i=1;i<treeHeight;i++) {
        nr.read(cpid,pf);
        rc = nr.locateChildPtr(searchKey,cpid);
    }
    BTLeafNode r;
    int endid;
    r.read(cpid,pf);
    rc =  r.locate(searchKey,endid);
    cursor.pid = cpid;
    cursor.eid = endid;
    curr = r;
    currPid = cursor.pid;
    return rc;
}

/*
 * Read the (key, rid) pair at the location specified by the index cursor,
 * and move foward the cursor to the next entry.
 * @param cursor[IN/OUT] the cursor pointing to an leaf-node index entry in the b+tree
 * @param key[OUT] the key stored at the index cursor location.
 * @param rid[OUT] the RecordId stored at the index cursor location.
 * @return error code. 0 if no error
 */
RC BTreeIndex::readForward(IndexCursor& cursor, int& key, RecordId& rid) {
    RC rc=0;
    int keystore;
    RecordId rstore;
    BTLeafNode r;
    if (currPid != cursor.pid) {
        r.read(cursor.pid,pf);
        currPid = cursor.pid;
        curr = r;
    }
    else {
        r = curr;
    }
    rc = r.readEntry(cursor.eid,keystore,rstore);
    key = keystore;
    rid = rstore;
    if (cursor.eid >= r.getKeyCount()-1) {
        cursor.eid = 0;
        PageId p = r.getNextNodePtr();
        cursor.pid = p;
        if (p == -1) {
            return -1;
        }
    }
    else {
        cursor.eid = cursor.eid+1;
    }
    return rc;
}
