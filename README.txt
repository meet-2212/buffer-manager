# LRURplacementPolicy

    -> The function uses fixCount to keep track of page usage.

    -> The function creates a frame and then fills the empty spaces(if any) present 
    in buffer pool also incrementing the fix count for the page and the number of occupied frames.

    -> Once, there is no space availabe, it looks for pages having fix count 0 so they can be replaced.

    -> Before replacing the page all the contents are written back to the file.

# ClockReplacementPolicy    

    -> Uses referenceBit to keep track of actively used pages.

    -> If space is availabe in the buffer pool, the spaces are filled first.

    -> Whenever a page is requested, and it is in buffer pool, the referenceBit is set to 1.

    -> After traversing the entire circular Linked list, the funtions looks for pages having
    referenceBit set to 0.

    -> If any such page if found, all of its content is written back to the disk and then is
    replaced by another page which is requested.


# getFrameContents

    -> Returns the actual data stored within the frames in the form of arrays of pageNumbers.
    -> Each array element represents the number of pages that are present in the respective frame.


# getDirtyFlags

    -> Returns the location of the dirty page within a page.
    -> Returns a Boolean array in which a page is marked TRUE if it is dirty.


# getFixCounts

    -> Returns array of integers of size numPages.
    -> Each array element represents the fix count of a page for the respective frame.


# getNumReadIO

    -> Returns the total number of read operations performed since the buffer pool is initilaized.



# getNumWriteIO

    -> Returns the total number of write operations performed since the buffer pool is initilaized.
