#include "utilities.h"
#include <mpi.h>
#include <iostream>

using std::cerr ;
using std::cout ;
using std::endl ;

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

void AllToAll(int send_value[], int recv_buffer[], int size, MPI_Comm comm){
    
    // MPI_Allgather(send_value,size,MPI_INT,recv_buffer,size,MPI_INT,comm) ;
    
    // MPI Primitives 
    MPI_Status status ; 
    int TAG_SEND = 0, TAG_RECV = 0 ; 

    int max_size = pow2(16) ;               // Maximum message size 
    int dimension = log2(numprocs) ;        // Hypercube dimension
    int proc_total = pow2(dimension) ;      // Physical & Virtual proc count
    int partner = 0 ;                       // Hypercube partner id 
    int virtual_id = 0 ;                    // Virtual id for non-physical procs
    int msg_size = size ;                   // Original message size 
    int i, j, k = 0 ;                       // Loop iterators   
    bool has_virtual_id = false ;           // Boolean to determine if physical proc has a virtual proc
    int host_proc = 0 ;                     // Physical processor id that's hosting a virtual processor          

    // Buffers
    int *physical_buffer = new int[proc_total*max_size] ;    // Buffer to store physical processors information
    int *virtual_buffer = new int[proc_total*max_size] ;     // Buffer to store virtual buffer hosted on physical proc
    int *recv_phys_msg ;                                     // Temp buffer to handle recv with physical procs
    int *recv_virt_msg ;                                     // Temp buffer to handle recv with virtual procs

    for (i=0; i<size; ++i) { 
        physical_buffer[i] = send_value[i] ;
        virtual_buffer[i] = send_value[i] ; 
    }

    // Check to see if the number of processors 
    // is a power of 2 
    if (numprocs > 0) {

        // Boolean to determine if the number of procs 
        // is a valid power of 2. 
        bool power2 = ((numprocs & (numprocs - 1)) == 0) ;
        
        // If the supplied amount of processors is 
        // not a power of 2 ... 
        if (!power2) {

            // Processor 0 will always be connected to physical processors;
            // therefore, we skip it. 
            if (myid != 0) {
                /*
                    Let us use the example of 3 procs. The next 
                    power of 2 will be 4. The mapped virtual processor
                    should be hosted on the lesser half of the 
                    dimension divide. 

                    2 --------- 3
                    |           |           
                    |           |
                ---------------------         
                    |           |
                    |           |
                    0 ---------- 1
                */

                // Find the next closest power of 2. 
                int next_pow2_dim = pow2(log2(numprocs)) ; 

                // Verify it falls into the lower half 
                // of the divide 
                if (myid < next_pow2_dim / 2) {

                    // Map the virtual proc to the appropriate physical proc
                    virtual_id = myid ^ (pow2(dimension-1)) ; 
                    has_virtual_id = true ; 

                } // end if (myid < next_pow2_dim / 2)
            } // end if (myid != 0) 
        } // end if (!power2) 
    } // end if (numprocs > 0) 

    // Perform All-To-All Hypercube Algorithm 
    for(i=0; i<dimension; ++i, size*=2) {
        
        // Determine the communication partner 
        partner = myid ^ pow2(i) ; 

        // Temp buffers to handle messages 
        recv_phys_msg = new int[numprocs*max_size];
        recv_virt_msg = new int[numprocs*max_size];
    
        // Current physical processor is hosting a virtual
        // procssor and attempting to communicate with the 
        // virtual proc. 
        if ((partner == virtual_id) && has_virtual_id) {

            // Append the virtual proc message 
            // before the physical proc message 
            if (myid > partner) {
                for (j=0; j<size; ++j) {
                    physical_buffer[j+size] = physical_buffer[j] ; 
                    physical_buffer[j] = virtual_buffer[j] ; 
                }
            }

            // Otherwise, append after the virtual proc
            // message 
            else {
                for (j=0; j<size; ++j) {
                    physical_buffer[j+size] = virtual_buffer[j] ; 
                }
            }
        }

        // Handle physical to physical and 
        // physical to virtual processors 
        else {

            // Physical proc to physical proc communication 
            if (numprocs > partner) {
                TAG_SEND = partner ; 
                TAG_RECV = myid ; 
                
                MPI_Sendrecv(physical_buffer, max_size*numprocs, MPI_INT, partner, TAG_SEND, 
                    recv_phys_msg, max_size*numprocs, MPI_INT, partner, TAG_RECV, comm, &status) ; 

                if (myid > partner) {
                    for (j=0; j<size; ++j) {
                        physical_buffer[j+size] = physical_buffer[j] ; 
                        physical_buffer[j] = recv_phys_msg[j] ; 
                    }
                }

                else {
                    for (j=0; j<size; ++j) {
                        physical_buffer[j+size] = recv_phys_msg[j] ; 
                    }
                }
            }

            // Physical proc to virtual proc communication 
            else {
                
                // Determine the physical processor that
                // has the virtual partner 
                host_proc = partner ^ pow2(dimension-1);
                TAG_SEND = partner ; 
                TAG_RECV = myid ; 
        
                // A physical processor needs to communicate with a virtual processor, so 
                // we must send the message to the physical (host) location. The host must
                // take on the role of the virtual process and handle the communication. 
                MPI_Sendrecv(physical_buffer, max_size*numprocs, MPI_INT, host_proc, TAG_SEND, 
                    recv_phys_msg, max_size*numprocs, MPI_INT, host_proc, TAG_RECV, comm, &status);

                if (myid > partner) {
                    for (j=0; j<size; ++j) {
                        physical_buffer[j+size] = physical_buffer[j] ; 
                        physical_buffer[j] = recv_phys_msg[j] ; 
                    }
                }

                else {
                    for (j=0; j<size; ++j) {
                        physical_buffer[j+size] = recv_phys_msg[j] ; 
                    }
                }
            }
        }

        // After this process has handled its physical process, 
        // the virtual process it is hosting must be handled. 
        if (has_virtual_id) {

            partner = virtual_id ^ pow2(i) ; 

            // Check to make sure this processor 
            // is not trying to communicate with 
            // the virtual processor it is hosting
            if (myid != partner) {

                // Virtual proc to physical proc communication 
                if (partner < numprocs) {
                    
                    TAG_SEND = partner ; 
                    TAG_RECV = virtual_id ; 

                    MPI_Sendrecv(virtual_buffer, max_size*numprocs, MPI_INT, partner, TAG_SEND, 
                        recv_virt_msg, max_size*numprocs, MPI_INT, partner, TAG_RECV, comm, &status);
                    
                    if (virtual_id > partner) {
                        for (j=0; j<size; ++j) {
                            virtual_buffer[j+size] = virtual_buffer[j] ; 
                            virtual_buffer[j] = recv_virt_msg[j] ; 
                        }
                    }

                    else {
                        for (j=0; j<size; ++j) {
                            virtual_buffer[j+size] = recv_virt_msg[j] ; 
                        }
                    }
                }

                // Check to see if the virtual processor
                // is attempting to communicate with
                // another virtual processor 
                else {
                    host_proc = partner ^ pow2(dimension - 1);
                    TAG_SEND = partner ; 
                    TAG_RECV = virtual_id ; 
                    
                    //cout << "before" << endl ; 
                    MPI_Sendrecv(virtual_buffer, max_size*numprocs, MPI_INT, host_proc, TAG_SEND, 
                        recv_virt_msg, max_size*numprocs, MPI_INT, host_proc, TAG_RECV, comm, &status);

                    //cout << "not stuck " << endl ; 
                    if (virtual_id > partner) {
                        for (j=0; j<size; ++j) {
                            virtual_buffer[j+size] = virtual_buffer[j] ; 
                            virtual_buffer[j] = recv_virt_msg[j] ; 
                        }
                    }

                    else {
                        for (j=0; j<size; ++j) {
                            virtual_buffer[j+size] = recv_virt_msg[j] ; 
                        }
                    }
                }
            }

            // The physical (hosting) process is communicating 
            // with the virtual (hosted) process. For this, we 
            // do not need to using a send/recv call because
            // the buffer is stored locally. 
            else {

                if (virtual_id > myid) {
                    for (j=0; j<size; ++j) {
                        virtual_buffer[j+size] = virtual_buffer[j] ; 
                        virtual_buffer[j] = physical_buffer[j] ; 
                    }
                }

                else {
                    for (j=0; j<size; ++j) {
                        virtual_buffer[j+size] = physical_buffer[j] ; 
                    }
                }
            }
        }

        delete[] recv_phys_msg ; delete[] recv_virt_msg;

    }

    // Relocate the buffer contents to the original
    // recv_buffer so that we can properly check the 
    // indices locations. 
    for (i = 0; i<numprocs*msg_size; ++i){
        recv_buffer[i] = physical_buffer[i];
    }

    delete[] physical_buffer ; delete[] virtual_buffer ;
}

/******************************************************************************
* This function should implement the All to All Personalized Broadcast.       *
* A value destined for each processor is given by the argument array          *
* send_buffer of size p.  The recv_buffer argument is an array of size p      *
* that will store the values that each processor transmits.                   *
* See pages 175-179 in the text.                                              *
******************************************************************************/

void AllToAllPersonalized(int send_buffer[], int recv_buffer[], int size, MPI_Comm comm) {
    
    // MPI_Alltoall(send_buffer,size,MPI_INT,recv_buffer,size,MPI_INT,comm) ;
    
    // Both flags CANNOT be set true at the same time. 
    bool run_e_cube_alg = false ; 
    bool run_mesh_alg = true ; 

    // MPI Primitives 
    MPI_Status status ; 
    int TAG_SEND = 0 ;
    int TAG_RECV = 0 ; 

    int partner = 0 ; 
    int max_size = pow2(16) ; 
    int i, j, k = 0 ;
    int dimension = log2(numprocs) ; 

    // Perform E-Cube Routing Algorithm 
    if (run_e_cube_alg) {

        for (i=1; i < numprocs; ++i) {
            
            partner = myid ^ i;

            MPI_Sendrecv(send_buffer, numprocs*max_size, MPI_INT, partner, TAG_SEND, 
                recv_buffer, numprocs*max_size, MPI_INT, partner, TAG_RECV, comm, &status);

            for (j=0; j<size; ++j) {
                send_buffer[partner*size + j] = recv_buffer[myid*size + j] ; 
            }
        }

        // Copy the final values to the recv_buffer
        for (i=0; i<size*numprocs; ++i) {
            recv_buffer[i] = send_buffer[i] ; 
        }
    }

    // Perform Mesh Algorithm 
    if (run_mesh_alg) {
 
        // This works similiarly to the setup of the 
        // traditional hypercube algorithm. 
        for (i=0; i<dimension; ++i) {

            // Establish partner id 
            partner = myid ^ pow2(i) ;
            
            
            int next = pow2(i) * size ; 

            MPI_Sendrecv(send_buffer, max_size*numprocs, MPI_INT, partner, TAG_SEND, 
                recv_buffer, max_size*numprocs, MPI_INT, partner, TAG_RECV, comm, &status) ; 

            // Perform the shuffle pattern
            // The data is contiguous, so we can shuffle information 
            // by its chunks of data
            if (myid < partner) {
                for (j=next; j<size*numprocs; j+=next*2) {
                    for (k=0; k<next; ++k) {
                        send_buffer[j+k] = recv_buffer[j-next+k] ; 
                    }
                }
            }
            else {
                for (j=0; j<size*numprocs; j+=next*2) {
                    for (k=0; k<next; ++k) {
                        send_buffer[j+k] = recv_buffer[j+k+next] ; 
                    }

                }
            }
        }

        // Copy the final values to the recv_buffer 
        for (i=0; i<size*numprocs; ++i) {
            recv_buffer[i] = send_buffer[i] ; 
        }
    }
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

    // Make sure we don't perform All-to-All personalized 
    // on non powers of 2
    if ((numprocs & (numprocs - 1)) != 0) {
        if (myid == 0) {
            cout << endl << "WARNING: Cannot perform All-to-All Personalized with non-power of 2 processors." << endl ; 
        }
        MPI_Finalize() ; 
        return 0; 
    }
    
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
