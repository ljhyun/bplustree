#include "BTreeNode.h"
#include <iostream>
#include <cstring>
using namespace std;



static int getRecordCount(const char* page) {
  int count;

  // the first four bytes of a page contains # records in the page
  memcpy(&count, page, sizeof(int));
  return count;
}


static void setRecordCount(char* page, int count)
{
  // the first four bytes of a page contains # records in the page
  memcpy(page, &count, sizeof(int));
}


/*
 * Read the content of the node from the page pid in the PageFile pf.
 * @param pid[IN] the PageId to read
 * @param pf[IN] PageFile to read from
 * @return 0 if successful. Return an error code if there is an error.
 */
BTLeafNode::BTLeafNode() {
    setRecordCount(buffer,0);
    
}

RC BTLeafNode::read(PageId pid, const PageFile& pf) { 
    RC rc;
    if ((rc = pf.read(pid,buffer)) < 0) {
        return rc;
    }
    memcpy(remap,buffer+sizeof(int),sizeof(remap[0])*getKeyCount());
    return 0; 
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::write(PageId pid, PageFile& pf) { 
    RC rc;
    memcpy(buffer+sizeof(int),remap,sizeof(remap[0])*getKeyCount());
    if ((rc=pf.write(pid,buffer)) < 0) {
        return rc;
    }
    return 0;
}

void BTLeafNode::testBuffer() {
    for(int i=0;i<getKeyCount();i++) {
        cout<<remap[i].key<<"|"<<remap[i].rec.sid<<"^";
    }    
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTLeafNode::getKeyCount() {
   return getRecordCount(buffer);
}

int BTLeafNode::getKey() {
    int key = remap[getKeyCount()-1].key;
    return key;
}

/*
 * Insert a (key, rid) pair to the node.
 * @param key[IN] the key to insert
 * @param rid[IN] the RecordId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTLeafNode::insert(int key, const RecordId& rid) {
   if (getKeyCount() >= TupleMax) {
        return RC_NODE_FULL;
   }
   else if (rid.pid < 0 || rid.sid < 0) {
        return RC_INVALID_RID;
   } 

   for(int i=0;i<getKeyCount();i++) {
       if(key <= remap[i].key) {
            for(int j=getKeyCount();j>i;j--) {
                remap[j] = remap[j-1];
            }
            remap[i].key = key;
            remap[i].rec = rid;
            setRecordCount(buffer,getKeyCount()+1);
            return 0;
        }
    }
    remap[getKeyCount()].key = key;
    remap[getKeyCount()].rec = rid;
    setRecordCount(buffer,getKeyCount()+1);
    return 0;
}

/*
 * Insert the (key, rid) pair to the node
 * and split the node half and half with sibling.
 * The first key of the sibling node is returned in siblingKey.
 * @param key[IN] the key to insert.
 * @param rid[IN] the RecordId to insert.
 * @param sibling[IN] the sibling node to split with. This node MUST be EMPTY when this function is called.
 * @param siblingKey[OUT] the first key in the sibling node after split.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::insertAndSplit(int key, const RecordId& rid, 
                              BTLeafNode& sibling, int& siblingKey) {

    if (sibling.getKeyCount() < 0) {
       return RC_INVALID_ATTRIBUTE;
    }
    else {
        int splitindex = getKeyCount()/2;
        if (remap[splitindex].key > key) {
            for(int ind = splitindex;ind<getKeyCount();ind++) {
                 sibling.insert(remap[ind].key,remap[ind].rec);
            }
            setRecordCount(buffer,getKeyCount()-(getKeyCount()+1)/2);
            siblingKey = remap[splitindex].key;
            insert(key,rid);
        }
        else {
            for(int ind = splitindex+1;ind<getKeyCount();ind++) {
                sibling.insert(remap[ind].key,remap[ind].rec);
            }
            if (getKeyCount() == splitindex+1 || remap[splitindex+1].key > key) {
                siblingKey = key;
            }
            else {
                siblingKey = remap[splitindex+1].key;
            }
            setRecordCount(buffer,getKeyCount()-(getKeyCount()-1)/2);
            sibling.insert(key,rid);
           }
        return 0; 
    }
}

/**
 * If searchKey exists in the node, set eid to the index entry
 * with searchKey and return 0. If not, set eid to the index entry
 * immediately after the largest index key that is smaller than searchKey,
 * and return the error code RC_NO_SUCH_RECORD.
 * Remember that keys inside a B+tree node are always kept sorted.
 * @param searchKey[IN] the key to search for.
 * @param eid[OUT] the index entry number with searchKey or immediately
                   behind the largest key smaller than searchKey.
 * @return 0 if searchKey is found. Otherwise return an error code.
 */
RC BTLeafNode::locate(int searchKey, int& eid) {
    if (searchKey == remap[0].key) {
        eid = 0;
        return 1;
    }
    else if (searchKey < remap[0].key) {
        eid = 0;
        return 1;
    }
    for(int i=1;i<getKeyCount();i++) {
        if(searchKey == remap[i].key) {
            eid = i;
            return 0;
        }
        else if (remap[i].key > searchKey) {
            eid = i-1;
            return RC_NO_SUCH_RECORD;
        }
    }
    eid = getKeyCount()-1;
    return RC_NO_SUCH_RECORD; 
}

/*
 * Read the (key, rid) pair from the eid entry.
 * @param eid[IN] the entry number to read the (key, rid) pair from
 * @param key[OUT] the key from the entry
 * @param rid[OUT] the RecordId from the entry
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::readEntry(int eid, int& key, RecordId& rid) {
    if (eid > getKeyCount()-1) {
        return RC_NO_SUCH_RECORD;
    }
    else {
        key = remap[eid].key;
        rid = remap[eid].rec;
        return 0;
    }
}

/*
 * Return the pid of the next slibling node.
 * @return the PageId of the next sibling node 
 */
PageId BTLeafNode::getNextNodePtr() {  
    int copy;
    memcpy(&copy,buffer+PageFile::PAGE_SIZE-sizeof(int),sizeof(int));
    return copy;    
}


/*
 * Set the pid of the next slibling node.
 * @param pid[IN] the PageId of the next sibling node 
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTLeafNode::setNextNodePtr(PageId pid) { 
    if (pid < -1) {
        return RC_INVALID_PID;
    } 
    else {
        memcpy(buffer+PageFile::PAGE_SIZE-sizeof(int),&pid,sizeof(int));
        return 0;
    }
}

 
BTNonLeafNode::BTNonLeafNode() {
    setRecordCount(buffer,0);
}

/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::read(PageId pid, const PageFile& pf) { 
    RC rc;
    if ((rc = pf.read(pid,buffer)) < 0) {
        return rc;
    }
    //TODO convert sizeof(int) to pointer size
    memcpy(repage,buffer+sizeof(int)+sizeof(int),sizeof(repage[0])*getKeyCount());
    return 0; 
}
    
/*
 * Write the content of the node to the page pid in the PageFile pf.
 * @param pid[IN] the PageId to write to
 * @param pf[IN] PageFile to write to
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::write(PageId pid, PageFile& pf) { 
    RC rc;
    //TODO convert sizeof(int) to pointer size
    memcpy(buffer+sizeof(int)+sizeof(int),repage,sizeof(repage[0])*getKeyCount());
    if ((rc=pf.write(pid,buffer)) < 0) {
        return rc;
    }
    return 0;
}

/*
 * Return the number of keys stored in the node.
 * @return the number of keys in the node
 */
int BTNonLeafNode::getKeyCount() {
    int count;
    memcpy(&count, buffer, sizeof(int));
    return count;
}

void BTNonLeafNode::testBuffer() {
    int rightkey;
    memcpy(&rightkey,buffer+sizeof(int), sizeof(int));
    cout<<rightkey<<"|";
    for(int i=0;i<getKeyCount();i++) {
        cout<<repage[i].key<<"|"<<repage[i].pid<<"|";
    }
    cout<<endl;
}

/*
 * Insert a (key, pid) pair to the node.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @return 0 if successful. Return an error code if the node is full.
 */
RC BTNonLeafNode::insert(int key, PageId pid) {
   if (getKeyCount()>=TupleMax) {
        return RC_NODE_FULL;
   }
   else if (pid < 0) {
       return RC_INVALID_PID;
   }
   for(int i=0;i<getKeyCount();i++) {
       if(key <= repage[i].key) {
            for(int j=getKeyCount();j>i;j--) {
                repage[j] = repage[j-1];
            }
            repage[i].key = key;
            repage[i].pid = pid;
            setRecordCount(buffer,getKeyCount()+1);
            return 0;
        }
    }
    repage[getKeyCount()].key = key;
    repage[getKeyCount()].pid = pid;
    setRecordCount(buffer,getKeyCount()+1);
    return 0;
}

/*
 * Insert the (key, pid) pair to the node
 * and split the node half and half with sibling.
 * The middle key after the split is returned in midKey.
 * @param key[IN] the key to insert
 * @param pid[IN] the PageId to insert
 * @param sibling[IN] the sibling node to split with. This node MUST be empty when this function is called.
 * @param midKey[OUT] the key in the middle after the split. This key should be inserted to the parent node.
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::insertAndSplit(int key, PageId pid, BTNonLeafNode& sibling, int& midKey) {
    if (sibling.getKeyCount() > 0) {
        return RC_NODE_FULL;
    }
    else if(pid < 0) {
        return RC_INVALID_PID;
    }
    else {
        int splitindex = (getKeyCount()+1)/2;
        if (key>repage[getKeyCount()-1].key)  {
            midKey = repage[splitindex].key;
            for(int ind=splitindex+1;ind<getKeyCount();ind++) {
                sibling.insert(repage[ind].key,repage[ind].pid);
            }
            sibling.insert(key,pid);
            sibling.setNextNodePtr(repage[getKeyCount()-1].pid);
            setRecordCount(buffer,getKeyCount()-getKeyCount()/2);
            return 0;
        }
        else {
            int tempkey = repage[getKeyCount()-1].key;
            PageId temppid = repage[getKeyCount()-1].pid;
            setRecordCount(buffer,getKeyCount()-1);
            insert(key,pid);
            midKey = repage[splitindex].key;
            for(int ind=splitindex+1;ind<getKeyCount();ind++) {
                sibling.insert(repage[ind].key,repage[ind].pid);
            }
            sibling.insert(tempkey,temppid);
            sibling.setNextNodePtr(repage[getKeyCount()-1].pid);
            setRecordCount(buffer,getKeyCount()-getKeyCount()/2);
            return 0;
        }      
    }
}


RC BTNonLeafNode::setNextNodePtr(PageId pid) {
    if (pid < 0) {
        return RC_INVALID_PID;
    }
    else {
        memcpy(buffer+sizeof(int),&pid,sizeof(int));
        return 0;
    }
}

RC BTNonLeafNode::locateChildPtr(int searchKey, PageId& pid) { 
    if (searchKey < repage[0].key) {
        memcpy(&pid,buffer+sizeof(int),sizeof(int));
        return 0;
    }
    for(int ind=1;ind<getKeyCount();ind++) {
        if (repage[ind].key >= searchKey) {
            pid = repage[ind-1].pid;
            return 0;
        }
    }
    pid = repage[getKeyCount()-1].pid;
    
    return 0;
}

/*
 * Initialize the root node with (pid1, key, pid2).
 * @param pid1[IN] the first PageId to insert
 * @param key[IN] the key that should be inserted between the two PageIds
 * @param pid2[IN] the PageId to insert behind the key
 * @return 0 if successful. Return an error code if there is an error.
 */
RC BTNonLeafNode::initializeRoot(PageId pid1, int key, PageId pid2) { 
    if (pid1<0 || pid2<0) {
        return RC_INVALID_PID;
    }
    memcpy(buffer+sizeof(int),&pid1,sizeof(int));
    insert(key,pid2);
    return 0; 
}

