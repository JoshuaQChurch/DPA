#include "utilities.h"
#include <mpi.h>
#include <iostream>
#include <vector> 
#include <algorithm> 

using std::cerr ;
using std::cout ;
using std::endl ;
using std::vector ; 
using std::copy ;

/******************************************************************************
* numprocs:  number of processors                                             *
* myid:      my processor number (numbered 0 - numprocs-1)                    *
******************************************************************************/
int numprocs, myid ; 

/******************************************************************************
* evaluate 2^i                                                                *
******************************************************************************/
inline unsigned int pow2(unsigned int i) { return 1 << i ; }

/******************************************************************************
* evaluate ceil(log2(i))                                                      *
******************************************************************************/
inline unsigned int log2(unsigned int i) {
    i-- ;
    unsigned int log = 1 ;
    for(i>>=1;i!=0;i>>=1)
        log++ ;
    return log ;
}

/******************************************************************************
* This function should implement the All to All Broadcast.  The value for each*
* processor is given by the argument send_value.  The recv_buffer argument is *
* an array of size p that will store the values that each processor transmits.*
* See Program 4.7 page 162 of the text.                                       *
******************************************************************************/

void AllToAll(int send_value[], int recv_buffer[], int size, MPI_Comm comm) {
    //MPI_Allgather(send_value,size,MPI_INT,recv_buffer,size,MPI_INT,comm) ;

    // MPI Primitives 
    MPI_Status status ;
    const int PHYSICAL_PROC = 0 ; 
    const int VIRTUAL_PROC = 1 ; 
    
    // Each processor has a buffer to store all messages
    // received, along with the initial message to 
    // be sent 
    vector<int> results (numprocs*size, -1) ; 

    // This ensures that the message is placed in the 
    // correct index position 
    int offset = myid*size ; 

    // Copy the id's initial message into the buffer 
    copy(send_value, send_value + size, results.begin() + offset) ;

    // The dimension of the hypercube 
    int dimension = log2(numprocs) ; 

    for (int i=0; i<dimension; ++i, size *=2) {
        int partner = myid ^ pow2(i) ; 

        MPI_Sendrecv(send_value, size, MPI_INT, partner, PHYSICAL_PROC,
            recv_buffer, size, MPI_INT, partner, PHYSICAL_PROC, comm, &status) ; 

        // Append the received values to the results buffer
        offset = partner*size ; 
        copy(recv_buffer, recv_buffer + size, results.begin() + offset) ;

        cout << "id: " << myid << endl;
        for (int k=0; k<size; ++k)
            cout << send_value[k] ; 
        
        cout << endl; 

    }

    // Copy the results buffer back into the received buffer 
    copy(results.begin(), results.end(), recv_buffer) ; 
    for (int i=0; i<size; ++i) {
        cout << recv_buffer[i] ; 
    }
    cout << endl ; 
  

    MPI_Finalize() ; 
    exit(0) ; 

    /*
        All-to-all broadcast on a Hypercube (Pseudocode)

        procedure ALL_TO_ALL_BC_HCUBE(my_id, my_msg, d, result) 
        begin 
            result := my_msg;
            for i:= 0 to d-1 do
                partner := my_id XOR 2^i; 
                send result to partner ;
                receive msg from partner ; 
                result := result U msg ; 
            endfor 
        end ALL_TO_ALL_BC_HCUBE ; 
    */
}

/******************************************************************************
* This function should implement the All to All Personalized Broadcast.       *
* A value destined for each processor is given by the argument array          *
* send_buffer of size p.  The recv_buffer argument is an array of size p      *
* that will store the values that each processor transmits.                   *
* See pages 175-179 in the text.                                              *
******************************************************************************/

void AllToAllPersonalized(int send_buffer[], int recv_buffer[], int size, MPI_Comm comm) {
    MPI_Alltoall(send_buffer,size,MPI_INT,recv_buffer,size,MPI_INT,comm) ;
}

int main(int argc, char **argv) {

    chopsigs_() ;
  
    double time_passes, max_time ;
    /* Initialize MPI */
    MPI_Init(&argc,&argv) ;

    /* Get the number of processors and my processor identification */
    MPI_Comm_size(MPI_COMM_WORLD,&numprocs) ;
    MPI_Comm_rank(MPI_COMM_WORLD,&myid) ;

    // Set the number of test runs 
    int test_runs = 8000/numprocs ;
    if(argc == 2) {
        test_runs = atoi(argv[1]) ;
    }

    // Upper bound on message size 
    const int max_size = pow2(16) ;

    // Allocate a buffer size large enough to 
    // store the maximum size of a single message 
    // send / received by the total number of processors
    int *recv_buffer = new int[numprocs*max_size] ;
    int *send_buffer = new int[numprocs*max_size] ;
  
    if (myid == 0) {
        cout << "Starting " << numprocs << " processors." << endl ;
    }

    /***************************************************************************/
    /* Check Timing for Single Node Broadcast emulating an alltoall broadcast  */
    /***************************************************************************/
    
    // Do not proceed until all processors 
    // are ready 
    MPI_Barrier(MPI_COMM_WORLD) ;

    // We can't accurately measure short times so we must execute this
    // operation many times to get accurate measurements 
    for(int l=0;l<=16;l+=4) {

        // Increase the message size by a factor of 4
        // for each iteration 
        int msize = pow2(l) ;

        // Start the timer 
        // Reset on each iteration 
        get_timer() ;

        // Loop until the test runs are completed
        for(int i=0;i<test_runs;++i) {

            // Slow All-to-All broadcast using p single node broadcasts
            for(int p=0;p<numprocs;++p) {
                recv_buffer[p] = 0 ;
            }

            // Create a unique message (int)
            int send_info = myid + i*numprocs ;

            // Populate the send buffer 
            // with the unique message 
            for(int k=0;k<msize;++k) {
                send_buffer[k] = send_info ;
            }

            // Perform the All-to-All broadcast algorithm 
            AllToAll(send_buffer,recv_buffer,msize,MPI_COMM_WORLD) ;

            // Verify that the received message matches 
            // what is to be expected 
            
            for(int p=0;p<numprocs;++p) {
	            if(recv_buffer[p*msize] != (p + i*numprocs)) {
                    cerr << "recv failed on processor " << myid << " recv_buffer["
                    << p << "] = "
                    << recv_buffer[p*msize] << " should  be " << p + i*numprocs << endl ;
                }
            }
        }
        
        // Get the amount of time that passes
        time_passes = get_timer() ;
  
        // Find the maximum time from all of the processors. 
        // This will give us an approximation for how long each
        // the all to all broadcast took for a particular message size
        MPI_Reduce(&time_passes, &max_time, 1, MPI_DOUBLE,MPI_MAX, 0, MPI_COMM_WORLD) ;
        if(myid == 0) {
            cout << "all to all broadcast for m="<< msize << " required " << max_time/double(test_runs)
            << " seconds." << endl ;
        }
    }

    MPI_Finalize(); 
    return 0; 
  
    /***************************************************************************/
    /* Check Timing for All to All personalized Broadcast Algorithm            */
    /***************************************************************************/

    // Barrier to ensure that we finish all of the following 
    // above, and that all the nodes are ready to proceed before
    // executing the All to All personlized algorithm. 
    MPI_Barrier(MPI_COMM_WORLD) ;

    for(int l=0;l<=16;l+=4) {
        int msize = pow2(l) ;
        /* Every call to get_timer resets the stopwatch.  The next call to 
        get_timer will return the amount of time since now */
        get_timer() ;

        for(int i=0;i<test_runs;++i) {
            for(int p=0;p<numprocs;++p) {
	            for(int k=0;k<msize;++k) {
                    recv_buffer[p*msize+k] = 0 ;
                }
            }
      
            int factor = (myid&1==1)?-1:1 ;
            for(int p=0;p<numprocs;++p) {
	            for(int k=0;k<msize;++k) {
                    send_buffer[p*msize+k]=myid*numprocs + p + i*myid*myid*factor;
                }
            }
            int send_info = myid + i*numprocs ;

            AllToAllPersonalized(send_buffer,recv_buffer,msize,MPI_COMM_WORLD) ;
    
            for(int p=0;p<numprocs;++p) {
                int factor = (p&1==1)?-1:1 ;
                if(recv_buffer[p*msize] != ( p*numprocs + myid + i*p*p*factor )) {
                    cerr << "recv failed on processor " << myid << " recv_buffer["
                    << p << "] = "
                    << recv_buffer[p*msize] << " should  be " << p*numprocs + myid + i*p*p*factor << endl ;
                }
            }
        }
  
        time_passes = get_timer() ;
  
        MPI_Reduce(&time_passes, &max_time, 1, MPI_DOUBLE,MPI_MAX, 0, MPI_COMM_WORLD) ;
        
        if(0 == myid) {
            cout << "all-to-all-personalized broadcast, m=" << msize 
            << " required " << max_time/double(test_runs)
            << " seconds." << endl ;
        }
    }

    delete[] recv_buffer ; 
    delete[] send_buffer ;

  
    /* We're finished, so call MPI_Finalize() to clean things up */
    MPI_Finalize() ;
    return 0 ;
}
