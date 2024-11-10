# SimpleSmartLoader - Jerovin Floyd, Aryan Verma


Me and Aryan have put almost 50-50 effort in making this assignment. We meet in library and do it in my laptop everyday.
In this assignment-4, we were asked to create a simpple smart loader, which is an upgraded version of the simple loader.
This program only loads segments when need while the program is being executed. This only works in a ELF-32 bit executable.


We had to use a signal handler for segmentation faults to implement the lazy loading.

We also made the internal fragmentation such that it tracks the unused space on the last page of a segment and displays its internal fragmentation.

There are so many situations of error handling we used throughout the code, in case of file opening failures, or memory allocation failures and so on.

The main function checks if the no. of arguments passed are correct and calls LoadELF to load and run the ELF.

We also created various other functions which serve different purposes.


GitHub Repo: https://github.com/JeroFloyd/SimpleSmartLoader.git
