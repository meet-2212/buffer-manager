#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer_mgr.h"
#include "storage_mgr.h"

typedef struct PageFrame
{
    int frameNum;                  // number associated with each Page frame
    int pageNum;                   // Page Number of the Page present in the Page frame
    int dirtyFlagSignal;           // determine if page was modified or not
    int fixCount;                  // Fix count to mark whether the page is in use by other users
    int referenceBit;              // Reference bit used in CLOCK Algorithm to mark the page which is referred
    char *data;                    // Actual data present in the page
    struct pageFrame *next, *prev; // Nodes of the Doubly linked List where each node is a frame, pointing to other frames
} PageFrame;

/*Structure for Buffer Pool to store Management Information*/
typedef struct BM_BufferPool_Mgmt
{
    int occupiedFrameCount;         // to keep count of number of frames occupied inside the pool
    void *replacementData;          // to pass parameters for page replacement strategies
    PageFrame *head, *tail, *start; // keep track of nodes in linked list
    PageNumber *frameContent;       // an array of page numbers to store the statistics of number of pages stored in the page frame
    int *fixCount;                  // an array of integers to store the statistics of fix counts for a page
    bool *markDirty;                // an array of bool's to store the statistics of dirty bits for modified page
    int getNumReadIO;               // to give total number of pages read from the buffer pool
    int getNumWriteIO;              // to give total number of pages wrote into the buffer pool
} BM_BufferPool_Mgmt;

/*
    # This function creates a buffer pool with a specified number of page frames, organized as a
      linked list.
    # Each frame has default values, with the first frame as the head and the last as the tail.
    # It is called by the initBufferPool() function, which passes the buffer management information.
*/
void createPageFrame(BM_BufferPool_Mgmt *mgmt)
{
    PageFrame *newFrame = (PageFrame *)malloc(sizeof(PageFrame));

    // Check if memory allocation was successful
    if (newFrame == NULL)
    {
        printf("Memory allocation for new frame failed.\n");
        return;
    }

    // Initialize the page properties for the new frame
    newFrame->dirtyFlagSignal = 0;
    newFrame->fixCount = 0;
    newFrame->frameNum = 0;
    newFrame->pageNum = -1;
    newFrame->referenceBit = 0;

    // Allocate memory for the page's data within the frame
    newFrame->data = (char *)calloc(PAGE_SIZE, sizeof(char));

    // Initialize the pointers to maintain the doubly linked list structure
    mgmt->head = mgmt->start;
    newFrame->next = NULL;
    newFrame->prev = NULL;

    // Check if the buffer pool is empty
    if (mgmt->head == NULL)
    {
        // The new frame becomes the head
        mgmt->head = newFrame;
        mgmt->tail = newFrame;
        // The start pointer also points to the new frame
        mgmt->start = newFrame;
    }
    else
    {
        // If the buffer pool already has frames, append the new frame to the tail
        // The current tail's next points to the new frame
        mgmt->tail->next = newFrame;
        // The new frame's previous points to the current tail
        newFrame->prev = mgmt->tail;
        // The new frame becomes the new tail
        mgmt->tail = newFrame;
    }
    // Tail's next points to the head to form a loop
    mgmt->tail->next = mgmt->head;
    // Head's previous points to the new tail
    mgmt->head->prev = mgmt->tail;
}

// Buffer Manager Interface Pool Handling
/*
    This function creates a buffer pool for an existing page file. It uses parameters:
    bm: Stores the mgmtData.
    pageFileName: The name of the page file whose pages will be cached.
    numPages: The number of frames in the buffer pool.
    strategy: The page replacement strategy to use.
    replacementData: Any additional parameters needed for the chosen page replacement strategy.
*/
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *replacementData)
{
    BM_BufferPool_Mgmt *bp_mgmt = (BM_BufferPool_Mgmt *)malloc(sizeof(BM_BufferPool_Mgmt));

    // Check if memory allocation was successful
    if (bp_mgmt == NULL)
    {
        printf("Memory allocation for buffer pool management failed.\n");
        // return RC_MEMORY_ALLOCATION_FAIL;
    }

    // Initialize the Storage Manager file handle
    SM_FileHandle fHandleandle;

    // Open the page file that will be cached in the buffer pool
    RC status = openPageFile((char *)pageFileName, &fHandleandle);
    if (status != RC_OK)
    {
        // Free allocated memory if file opening fails
        free(bp_mgmt);
        // Return the error code from file handling
        return status;
    }

    // Create the frames for the buffer pool
    for (int i = 0; i < numPages; i++)
    {
        createPageFrame(bp_mgmt);
    }

    // Initialize buffer pool management data
    // Set tail to head after creating frames
    bp_mgmt->tail = bp_mgmt->head;
    // Set the strategy data
    bp_mgmt->replacementData = replacementData;
    // No frames are occupied initially
    bp_mgmt->occupiedFrameCount = 0;
    // No pages read initially
    bp_mgmt->getNumReadIO = 0;
    // No pages written initially
    bp_mgmt->getNumWriteIO = 0;

    // Initialize buffer pool structure
    // Set the number of pages
    bm->numPages = numPages;
    // Store the page file name
    bm->pageFile = strdup(pageFileName);
    // Set the replacement strategy
    bm->strategy = strategy;
    // Set the management data
    bm->mgmtData = bp_mgmt;

    // Close the page file after initialization
    closePageFile(&fHandleandle);

    return RC_OK;
}

/*
    # This function shuts down the buffer pool and frees associated resources.
*/
RC shutdownBufferPool(BM_BufferPool *const bm)
{
    // Load the management data of the buffer pool
    if (bm == NULL || bm->mgmtData == NULL)
    {
        // Return if buffer pool or management data is invalid
        // return RC_INVALID_INPUT;
    }

    BM_BufferPool_Mgmt *bp_mgmt = (BM_BufferPool_Mgmt *)bm->mgmtData;

    // Ensure the buffer pool is not already empty
    if (bp_mgmt->head == NULL)
    {
        // No frames to free, already empty
        return RC_OK;
    }

    // Point to the head node
    PageFrame *currentFrame = bp_mgmt->head;

    RC status = forceFlushPool(bm);
    if (status != RC_OK)
    {
        // Return error if flush fails
        return status;
    }

    // Free all the page data in each frame, traversing the circular linked list
    do
    {
        PageFrame *temp = currentFrame;
        currentFrame = currentFrame->next;

        // Free the data in the frame and then the frame itself
        // Free the page data
        free(temp->data);
        // Free the frame
        free(temp);
    } while (currentFrame != bp_mgmt->head);

    // Nullify all the management pointers and reset the buffer pool structure
    bp_mgmt->start = NULL;
    bp_mgmt->head = NULL;
    bp_mgmt->tail = NULL;

    // Free the buffer pool management structure
    free(bp_mgmt);

    // Set all buffer pool values to 0 or NULL
    bm->numPages = 0;
    bm->mgmtData = NULL;
    bm->pageFile = NULL;

    return RC_OK;
}
/*
    This function flushes any dirty pages in the buffer pool to disk.
*/
RC forceFlushPool(BM_BufferPool *const bm)
{
    // Check for valid buffer pool and management data
    if (bm == NULL || bm->mgmtData == NULL)
    {
        // return RC_INVALID_INPUT;
    }

    // Load the management data
    BM_BufferPool_Mgmt *bp_mgmt = (BM_BufferPool_Mgmt *)bm->mgmtData;

    // Point to the head of the circular linked list of frames
    PageFrame *currentFrame = bp_mgmt->head;

    // Open the page file
    SM_FileHandle fHandleandle;
    if (openPageFile((char *)bm->pageFile, &fHandleandle) != RC_OK)
    {
        return RC_FILE_NOT_FOUND;
    }

    // Traverse through the circular linked list and check for dirty pages to flush
    do
    {
        if (currentFrame->dirtyFlagSignal == true && currentFrame->fixCount == 0)
        {
            // Write the dirty page back to disk
            if (writeBlock(currentFrame->pageNum, &fHandleandle, currentFrame->data) != RC_OK)
            {
                // Ensure the file is closed before returning
                closePageFile(&fHandleandle);
                return RC_WRITE_FAILED;
            }

            // Mark the frame as clean (not dirty anymore)
            currentFrame->dirtyFlagSignal = false;
            // Increment the write count
            bp_mgmt->getNumWriteIO++;
        }

        // Move to the next frame in the linked list
        currentFrame = currentFrame->next;
        // Stop when we return to the head frame
    } while (currentFrame != bp_mgmt->head);

    // Close the page file after all dirty pages are written
    closePageFile(&fHandleandle);

    return RC_OK;
}

/*Function to create a PageFrame*/

// Buffer Manager Interface Access Pages

/* This function is used to mark a page dirty when it is modified by the user and it is set to 1  */
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    // check if the page is present in the buffer pool and get the management info of the buffer pool
    BM_BufferPool_Mgmt *bp_mgmt;
    bp_mgmt = bm->mgmtData;
    // get the head node of the linked list of frames
    PageFrame *frame = bp_mgmt->head;

    // check if the page number is present in the linked list
    do
    {
        // check if the pageNum is same as the page to be marked dirty
        if (page->pageNum == frame->pageNum)
        {
            // mark the page as dirty and return success
            frame->dirtyFlagSignal = 1;
            return RC_OK;
        }
        // go to next frame
        frame = frame->next;
    } while (frame != bp_mgmt->head);

    // if not found return error
    return RC_OK;
}

/* This function is used to unpin a page from the buffer pool when it is no longer in use by the user means it is set to free
this method called as "unpinning" a page in the buffer pool */
RC unpinPage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    // check if the page is present in the buffer pool and get the management info of the buffer pool
    BM_BufferPool_Mgmt *bp_mgmt;
    bp_mgmt = bm->mgmtData;
    PageFrame *frame = bp_mgmt->head;

    // check if the page number is present in the linked list
    do
    {
        // check if the pageNum is the same as the page to be unpinned
        if (page->pageNum == frame->pageNum)
        {
            // decrement the fix count and return success if it is 0
            frame->fixCount--;
            return RC_OK;
        }
        // go to next frame if not found
        frame = frame->next;
    } while (frame != bp_mgmt->head);

    // if not found return error
    return RC_OK;
}

/* This function is used to pin a page passed to it in BM_PageHandle and writes back to the disk file
    when it is no longer in use by the user means it is set to free */
RC forcePage(BM_BufferPool *const bm, BM_PageHandle *const page)
{
    // check if the page is present in the buffer pool and get the management info of the buffer pool
    BM_BufferPool_Mgmt *bp_mgmt;
    bp_mgmt = bm->mgmtData;
    // get the head node of the linked list of frames
    PageFrame *Frame = bp_mgmt->head;
    SM_FileHandle fHandle;

    // open the file for writing
    if (openPageFile((char *)(bm->pageFile), &fHandle) != RC_OK)
    {
        // if not found return error
        return RC_FILE_NOT_FOUND;
    }

    // find the page that needs to be written back to the disk file and
    // check if its dirty flag is set to 1 and write it back
    do
    {
        if (Frame->pageNum == page->pageNum && Frame->dirtyFlagSignal == 1)
        {
            // check if the write is successful or not and return error if not
            if (writeBlock(Frame->pageNum, &fHandle, Frame->data) != RC_OK)
            {
                closePageFile(&fHandle);
                return RC_WRITE_FAILED;
            }
            // update the number of writes done
            bp_mgmt->getNumWriteIO++;
            // set the dirty flag to 0
            Frame->dirtyFlagSignal = 0;
            break;
        }
        // go to next frame if not found
        Frame = Frame->next;
    } while (Frame != bp_mgmt->head);

    //  close the file and return success
    closePageFile(&fHandle);
    return RC_OK;
}

/* This function is used to put the page in the buffer pool and each page is put in page frame which is put into bufferPool and this method called as "pinning" a page in the buffer pool*/
RC pinPage(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
    // chooes the pinning strategy based on the buffer pool strategy
    // there are three strategies FIFO, LRU and CLOCK which are implemented in different functions
    switch (bm->strategy)
    {
    case RS_FIFO:
        pinPageFIFO(bm, page, pageNum);
        break;

    case RS_LRU:
        pinPageLRU(bm, page, pageNum);
        break;

    case RS_CLOCK:
        pinPageCLOCK(bm, page, pageNum);
        break;
    }
    return RC_OK;
}

// This function pinPageFIFO is First in First out (FIFO) pinning strategy, In this we have implemented FIFO using queue data structure
RC pinPageFIFO(BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum)
{
    // SM_FileHandle is used to open the page file
    SM_FileHandle fHandle;
    // get the management info of the buffer pool
    BM_BufferPool_Mgmt *bp_mgmt = bm->mgmtData;
    // this is used to get the head node of the linked list of frames
    PageFrame *frame = bp_mgmt->head;

    // open the page file
    openPageFile((char *)bm->pageFile, &fHandle);

    // check if the page is present in the buffer pool and get the management info of the buffer pool
    do
    {
        // check if the pageNum is the same as the page to be pinned
        if (frame->pageNum == pageNum)
        {
            // update the page number
            page->pageNum = pageNum;
            // update the page data
            page->data = frame->data;

            // update the frame and pageNum and increment the fix count
            frame->pageNum = pageNum;
            frame->fixCount++;
            return RC_OK;
        }
        // go to next frame if not found
        frame = frame->next;
    } while (frame != bp_mgmt->head);

    // check if the buffer pool is not full and pin the page in empty space else use page replacement strategy
    if (bp_mgmt->occupiedFrameCount < bm->numPages)
    {
        frame = bp_mgmt->head;
        frame->pageNum = pageNum;

        // move the header to next empty space and increment the fixCount
        if (frame->next != bp_mgmt->head)
        {
            bp_mgmt->head = frame->next;
        }
        frame->fixCount++;
        bp_mgmt->occupiedFrameCount++;
    }
    else
    {
        // replace pages from frame whose fixcount = 0
        frame = bp_mgmt->tail;
        do
        {
            // check if the frame is in use  if fixCount > 0
            if (frame->fixCount != 0)
            {
                // update the frame
                frame = frame->next;
            }
            // go to next frame whoes count is fixCount = 0 and replce the page
            else
            {
                // before replacing check for dirty flag if it is writes back to disk then repclace it
                if (frame->dirtyFlagSignal == 1)
                {
                    // write the block to disk
                    ensureCapacity(frame->pageNum, &fHandle);
                    // check if the write is successful or not and return error if not
                    if (writeBlock(frame->pageNum, &fHandle, frame->data) != RC_OK)
                    {
                        closePageFile(&fHandle);
                        return RC_WRITE_FAILED;
                    }
                    bp_mgmt->getNumWriteIO++;
                }

                // update the frame and pageNum and increment the fix count
                frame->pageNum = pageNum;
                frame->fixCount++;
                // update the tail and move the tail to next frame
                bp_mgmt->tail = frame->next;
                bp_mgmt->head = frame;

                break;
            }
        } while (frame != bp_mgmt->head);
    }

    // check if the pageFile has the required number of pages if not create those pages
    ensureCapacity((pageNum + 1), &fHandle);

    // read the block into pageFrame data
    if (readBlock(pageNum, &fHandle, frame->data) != RC_OK)
    {
        // if not found return error
        closePageFile(&fHandle);
        return RC_READ_NON_EXISTING_PAGE;
    }

    // increment the num of read operations
    bp_mgmt->getNumReadIO++;

    // update the pageNum and data of the page
    page->pageNum = pageNum;
    page->data = frame->data;

    // close the pageFile
    closePageFile(&fHandle);

    return RC_OK;
}

/*
    # Implementation for LRU page Replacement policy.
    # Keeps track of fic count to see the usage of page.
    # replaces the pages having fix count as 0.
*/

RC pinPageLRU(BM_BufferPool *const bm, BM_PageHandle *const currPage, const PageNumber pageNum)
{
    BM_BufferPool_Mgmt *buffPoolMgmt = (*bm).mgmtData;
    PageFrame *frame = (*buffPoolMgmt).head;
    SM_FileHandle fHandleandler;

    openPageFile((char *)(*bm).pageFile, &fHandleandler);

    // check if the frame is already in the bufferpool
    do
    {
        if ((*frame).pageNum == pageNum)
        {
            // update the page and frame attributes
            (*currPage).pageNum = pageNum;
            (*currPage).data = (*frame).data;

            (*frame).pageNum = pageNum;
            (*frame).fixCount++;

            // point the head and tail for replacement
            (*buffPoolMgmt).tail = (*buffPoolMgmt).head->next;
            (*buffPoolMgmt).head = frame;
            return RC_OK;
        }

        frame = (*frame).next;

    } while (frame != (*buffPoolMgmt).head);

    // check if space is availabe if bufferpool
    // if yes then fill it
    if ((*buffPoolMgmt).occupiedFrameCount < (*bm).numPages)
    {

        frame = (*buffPoolMgmt).head;
        (*frame).pageNum = pageNum;

        if ((*frame).next != (*buffPoolMgmt).head)
        {
            (*buffPoolMgmt).head = (*frame).next;
        }
        // increment the fix count
        (*frame).fixCount++;

        // increment number of occupied frames
        (*buffPoolMgmt).occupiedFrameCount++;
    }
    else
    {
        // replace pages from frame using LRU
        frame = (*buffPoolMgmt).tail;
        do
        {
            //  check for page usage, if in use move to next
            if ((*frame).fixCount != 0)
            {
                frame = (*frame).next;
            }
            else
            {
                // check for dirtyFlagSigbnal if it is 1, write page to disk before replacing
                if ((*frame).dirtyFlagSignal == 1)
                {
                    ensureCapacity((*frame).pageNum, &fHandleandler);
                    if (writeBlock((*frame).pageNum, &fHandleandler, (*frame).data) != RC_OK)
                    {
                        closePageFile(&fHandleandler);
                        return RC_WRITE_FAILED;
                    }
                    // increment writes performed
                    (*buffPoolMgmt).getNumWriteIO++;
                }

                // find the least recently used page and replace that page
                if ((*buffPoolMgmt).tail != (*buffPoolMgmt).head)
                {
                    (*frame).pageNum = pageNum;
                    (*frame).fixCount++;
                    (*buffPoolMgmt).tail = frame;
                    (*buffPoolMgmt).tail = (*frame).next;
                    break;
                }
                else
                {
                    frame = (*frame).next;
                    (*frame).pageNum = pageNum;
                    (*frame).fixCount++;
                    (*buffPoolMgmt).tail = frame;
                    (*buffPoolMgmt).head = frame;
                    (*buffPoolMgmt).tail = (*frame).prev;
                    break;
                }
            }
        } while (frame != (*buffPoolMgmt).tail);
    }

    ensureCapacity((pageNum + 1), &fHandleandler);
    if (readBlock(pageNum, &fHandleandler, (*frame).data) != RC_OK)
    {
        return RC_READ_NON_EXISTING_PAGE;
    }
    (*buffPoolMgmt).getNumReadIO++;

    // update the page frame and its data
    (*currPage).pageNum = pageNum;
    (*currPage).data = (*frame).data;

    // close the pagefile
    closePageFile(&fHandleandler);

    return RC_OK;
}

/*
    # Implementation for CLOCK page Replacement policy.
    # Uses FIFO in circular queue along with setting referenceBit for every pages in frame
 */
RC pinPageCLOCK(BM_BufferPool *const bm, BM_PageHandle *const currPage, const PageNumber pageNum)
{
    SM_FileHandle fHandleandler;
    BM_BufferPool_Mgmt *buffPoolMgmt = (*bm).mgmtData;
    PageFrame *frame = (*buffPoolMgmt).head;
    PageFrame *temp;
    openPageFile((char *)(*bm).pageFile, &fHandleandler);

    // if frame already in buffer pool
    do
    {
        // check if the page is referred again
        if ((*frame).pageNum == pageNum)
        {
            (*currPage).pageNum = pageNum;
            (*currPage).data = (*frame).data;

            // mark its reference bit as 1
            (*frame).referenceBit = 1;
            (*frame).pageNum = pageNum;
            (*frame).fixCount++;

            return RC_OK;
        }
        frame = (*frame).next;
    } while (frame != (*buffPoolMgmt).head);

    // when space is available execution from the start only if all the frames are empty
    if ((*buffPoolMgmt).occupiedFrameCount < (*bm).numPages)
    {
        frame = (*buffPoolMgmt).head;

        (*frame).pageNum = pageNum;
        // mark their reference bit as 1
        (*frame).referenceBit = 1;

        // all insertions are at head
        // after insertion move to next node
        if ((*frame).next != (*buffPoolMgmt).head)
        {
            (*buffPoolMgmt).head = (*frame).next;
        }

        (*frame).fixCount++;
        (*buffPoolMgmt).occupiedFrameCount++;
    }
    else
    {
        // if page replacement is needed
        frame = (*buffPoolMgmt).head;

        do
        {
            // look for page with referenceBit set to 0 so it can be replaced
            if ((*frame).fixCount != 0)
            {
                frame = (*frame).next;
            }
            else
            {
                while ((*frame).referenceBit != 0)
                {
                    (*frame).referenceBit = 0;
                    frame = (*frame).next;
                }

                // check if reference bit is 0
                if ((*frame).referenceBit == 0)
                {
                    // check if the dirtyFlagSiganl is set
                    // if yes, write to disk and then replace
                    if ((*frame).dirtyFlagSignal == 1)
                    {
                        ensureCapacity((*frame).pageNum, &fHandleandler);
                        if (writeBlock((*frame).pageNum, &fHandleandler, (*frame).data) != RC_OK)
                        {
                            closePageFile(&fHandleandler);
                            return RC_WRITE_FAILED;
                        }
                        (*buffPoolMgmt).getNumWriteIO++;
                    }
                    // update all the frame along with page attributes
                    (*frame).referenceBit = 1;
                    (*frame).pageNum = pageNum;
                    (*frame).fixCount++;
                    (*buffPoolMgmt).head = (*frame).next;
                    break;
                }
            }

        } while (frame != (*buffPoolMgmt).head);
    }
    ensureCapacity((pageNum + 1), &fHandleandler);

    if (readBlock(pageNum, &fHandleandler, (*frame).data) != RC_OK)
    {
        closePageFile(&fHandleandler);
        return RC_READ_NON_EXISTING_PAGE;
    }

    (*buffPoolMgmt).getNumReadIO++;
    (*currPage).pageNum = pageNum;
    (*currPage).data = (*frame).data;

    closePageFile(&fHandleandler);

    return RC_OK;
}

// ------------- Method Implementation for Statistics Interface -------------

/*
    # The following funtion returns array of PageNumbers having of numPages.
    # Any element in the array represents the number of pages stored in the respective page frame.
    # A frame with no pages is represented using NO_PAGE constant.
 */
PageNumber *getFrameContents(BM_BufferPool *const bm)
{
    BM_BufferPool_Mgmt *buffPoolMgmt;
    buffPoolMgmt = (*bm).mgmtData;
    (*buffPoolMgmt).frameContent = (PageNumber *)malloc(sizeof(PageNumber) * (*bm).numPages);

    PageFrame *frame = (*buffPoolMgmt).start;
    PageNumber *frameContents = (*buffPoolMgmt).frameContent;

    int i;
    int page_count = (*bm).numPages;

    if (frameContents != NULL)
    {
        for (i = 0; i < page_count; i++)
        {
            frameContents[i] = (*frame).pageNum;
            frame = (*frame).next;
        }
    }

    return frameContents;
}

/*
    # The getDirtyFlags returns boolen arrays of size numPages
    # If a page frame is dirty then its corresponding elemet is set TRUE
    # All empty page frames are clean hence is set to FALSE
 */
bool *getDirtyFlags(BM_BufferPool *const bm)
{
    BM_BufferPool_Mgmt *buffPoolMgmt;
    buffPoolMgmt = (*bm).mgmtData;
    (*buffPoolMgmt).markDirty = (bool *)malloc(sizeof(bool) * (*bm).numPages);

    PageFrame *frame = (*buffPoolMgmt).start;
    bool *markDirty = (*buffPoolMgmt).markDirty;

    int i, page_count = (*bm).numPages;

    if (markDirty != NULL)
    {
        for (i = 0; i < page_count; i++)
        {
            markDirty[i] = (*frame).dirtyFlagSignal;
            frame = (*frame).next;
        }
    }

    return markDirty;
}

/*
    # The function below returns int arrays of size numPages
    # The ith element is stored in ith frame as it is the fix count for thr page
 */
int *getFixCounts(BM_BufferPool *const bm)
{
    BM_BufferPool_Mgmt *buffPoolMgmt;
    buffPoolMgmt = (*bm).mgmtData;
    (*buffPoolMgmt).fixCount = (int *)malloc(sizeof(int) * (*bm).numPages);

    PageFrame *frame = (*buffPoolMgmt).start;
    int *fixCount = (*buffPoolMgmt).fixCount;

    int i, page_count = (*bm).numPages;

    if (fixCount != NULL)
    {
        for (i = 0; i < page_count; i++)
        {
            fixCount[i] = (*frame).fixCount;
            frame = (*frame).next;
        }
    }
    // free((*buffPoolMgmt).fixCount);

    return fixCount;
}

/*
    # The function below gets the total number of Read Operations
 */
int getNumReadIO(BM_BufferPool *const bm)
{
    return ((BM_BufferPool_Mgmt *)(*bm).mgmtData)->getNumReadIO;
}

/*
    # This function below gets the total number of Read Operations
 */
int getNumWriteIO(BM_BufferPool *const bm)
{
    return ((BM_BufferPool_Mgmt *)(*bm).mgmtData)->getNumWriteIO;
}