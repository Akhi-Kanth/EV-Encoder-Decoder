My first idea was is to start with the numbers. Since the numbers make up the majorty of the code I want to minimize that first. 


My idea for that is to note somewhere in the encoded file the starting value (so that if it is subject to change my code will still be able to handle it)
i will mark it with some ascci say x. then in the next line if it is incremented from the past line, i store it as x1, if it stays the same then it is x0. 

With that idea with simplified 6-7 digits down to two digits. 

Next is the 4 digit code that always starts with 8192-9215. 


now im taking a pause becuase i am reading about compiled binaries which could be useful to futher drive down the data. The instructions said that it should
be debuggable with hexdump, so I am thinking that a complied binary is the way to go. Before i was just going to save all the data in a text file and that still might be too much space.


we are going line by line so 
we have [time], [value numbers], [value]