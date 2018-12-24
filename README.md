# stickpic

This is a simple tool to generate a map of file or block device content compressibility in the form of a png image.
It uses the minilzo library to work out the compressibility of a block and translates this into one of three optional colormaps.

This is useful to get a visual impression about where the data is on a disk drive, what type of data it is, how filesystems are behaving and how or if TRIM is working (on SSDs). It is also useful to see thin provisioning in effect on SD cards.

While TRIM usage in SSDs is generally working and people have a general idea about this, SD cards can also benefit from occasional trims. The wear levelling will have less pressure put on it and the cards will in general be a little bit faster.
It is important to understand that the trimming (I usually use Linux blkdiscard on the SD device) will not work when the cards is accessed through typical card readers, simulating a USB stick. I have been successful using blkdiscard and fstrim when such cards are used within a direct SD card slot in a notebook computer, or in a raspberry pi SD slot.
Also note that some cameras and video cameras can 'format' the card. This usually is a fast process and it leaves no trace of data accessible on the cards and is probably using the same thin provisioning trim-type operation as blkdiscard and fstrim. The data is then still inside the flash but not accessible anymore since the memory controller will mark the flash as 'free/dont care' and returns zeros very quickly when such blocks are read.
All SD cards in my possession can do the blkdiscard. It seems like they all implement this function. The Windows SD formatter tool by the SD consortium tries to do a blkdiscard on the devices, but from my observations it also cannot do it through standard card readers.

All of these things on SD cards and SSDs can be visualized using the stickpic tool. Just take a 'snapshot' before and after a trim or blkdiscard.

The tool will only read over your data and compress every block for compressibility assessment.