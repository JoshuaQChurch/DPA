#include "game.h"
#include "utilities.h"
#include "string.h"
// Standard Includes for MPI, C and OS calls
#include <mpi.h>

// C++ standard I/O and library includes
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// C++ stadard library using statements
using std::cout ;
using std::cerr ;
using std::endl ;

using std::vector ;
using std::string ;

using std::ofstream ;
using std::ifstream ;
using std::stringstream ;
using std::ios ;

// Globals
const unsigned int TAG_SOLVE = 1;           // Server tag telling client to solve what's in buffer
const unsigned int TAG_SOLUTION = 2;        // Client tag telling server that solution is in buffer
const unsigned int TAG_NO_SOLUTION = 3;     // Client tag telling server that no solution was found
const unsigned int TAG_FINISHED = 4;        // Server tag telling all clients to end communication
const unsigned int TAG_READY = 5;           // Client tag telling server it is ready for jobs
unsigned int BOARD_SIZE = IDIM*JDIM;        // Size of the game board
MPI_Request request;                        // MPI request handle
MPI_Status status;                          // MPI status handle

// The server should delegate work to all the clients.
// However, while the server is idle, it should also
// do a puzzle to utilize the available resources wisely.
void Server(int argc, char *argv[], int procs) {

    // Check to make sure the server can run
    if(argc != 3) {
        cerr << "two arguments please!" << endl ;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    ifstream input(argv[1],ios::in);    // Input case filename
    ofstream output(argv[2],ios::out);  // Output case filename

    int received = 0;                   // Flag to catch if message received from client
    unsigned int solutions = 0;         // Total number of solutions
    unsigned int NUM_GAMES = 0 ;        // Total number of games read in from the file
    input >> NUM_GAMES ;                // Get games from input file
    int i = 0;                          // Game counter

    // For each game in the provided file, do the following...
    while (i < NUM_GAMES){

        unsigned char buffer[NUM_GAMES*BOARD_SIZE];

        // Listen for message from any of the client procs
        MPI_Irecv(buffer, NUM_GAMES*BOARD_SIZE, MPI_UNSIGNED_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
        MPI_Test(&request, &received, &status);

        // While the server hasn't received anything, perform a job
        while (!received && i < NUM_GAMES) {

            // Get the next line in the file
            string input_string;
            input >> input_string;
            ++i;

            // Buffer to store current game board
            unsigned char server_buffer[BOARD_SIZE];
            for (int j=0; j<BOARD_SIZE; ++j)
                server_buffer[j] = input_string[j];

            // Solve a game
            game_state game_board;
            game_board.Init(server_buffer) ;

            move solution[BOARD_SIZE];
            int size=0;
            bool found = depthFirstSearch(game_board, size, solution);

            if (found) {
                output << "found solution = " << endl;
                game_state s;
                s.Init(server_buffer);
                s.Print(output);
                for (int k=0; k<size; ++k) {
                    s.makeMove(solution[k]);
                    output << "-->" << endl;
                    s.Print(output);
                }
                output << "solved" << endl;
                ++solutions;
            }

            // Check again
            MPI_Test(&request, &received, &status);
        }

        // We have received something from a client proc,
        // handle it.
        if (received) {

            // What type of message and whom
            // it came from.
            int source = status.MPI_SOURCE;
            int tag = status.MPI_TAG;

            // If the client found a solution...
            if (tag == TAG_SOLUTION) {
                for (int k=0; k<sizeof(buffer); k++)
                  cout << buffer[k];
                ++solutions;
            }

            // Get the next line in the file
            string input_string;
            input >> input_string;
            ++i;

            // Buffer to store current game board
            memset(buffer, 0, BOARD_SIZE);
            for (int j=0; j<BOARD_SIZE; ++j)
                buffer[j] = input_string[j];

            // Send another job to the client
            int client_received = 0;
            MPI_Isend(buffer, BOARD_SIZE, MPI_UNSIGNED_CHAR, source, TAG_SOLVE, MPI_COMM_WORLD, &request);
            MPI_Test(&request, &client_received, &status);

            // Make sure client receives job
            // Do another job until then
            while (!client_received && i < NUM_GAMES) {
                string input_string;
                input >> input_string;
                ++i;

                // Buffer to store current game board
                unsigned char server_buffer[BOARD_SIZE];
                for (int j=0; j<BOARD_SIZE; ++j)
                    server_buffer[j] = input_string[j];

                // Prep game board and solve another
                game_state game_board;
                game_board.Init(server_buffer);
                move solution[BOARD_SIZE];
                int size=0;
                bool found = depthFirstSearch(game_board, size, solution);

                // Add to the output stream
                if (found) {
                    output << "found solution = " << endl;
                    game_state s;
                    s.Init(server_buffer);
                    s.Print(output);
                    for (int i = 0; i < size; ++i) {
                        s.makeMove(solution[i]);
                        output << "-->" << endl;
                        s.Print(output);
                    }
                    output << "solved" << endl;
                    solutions++;
                }

                // Test again.
                MPI_Test(&request, &client_received, &status);
            }
        } // End if received
    } // End NUM_GAMES while loop

    // All games have been handled, end communication
    // between all client procs
    for (int i=1; i<procs; i++) {
        unsigned char buffer[1];
        MPI_Isend(buffer, 0, MPI_UNSIGNED_CHAR, i, TAG_FINISHED, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);
    }

    // Report how cases had a solution.
    cout << "found " << solutions << " solutions" << endl ;
}

void Client() {

    // When ready, send initial 'ready' tag to
    // begin communication with the server.
    unsigned char buffer[BOARD_SIZE];
    MPI_Isend(buffer, 0, MPI_UNSIGNED_CHAR, 0, TAG_READY, MPI_COMM_WORLD, &request);
    MPI_Wait(&request, MPI_SUCCESS);

    // Now that job has been received, continue to
    // do work until a 'finished' tag has been received.
    while (true) {
        unsigned char buffer[BOARD_SIZE];

        // Wait until game is fully received before trying to solve.
        MPI_Irecv(buffer, BOARD_SIZE, MPI_UNSIGNED_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, &status);

        // If 'finished' tag received, stop communication
        if (status.MPI_TAG == TAG_FINISHED) { break; }

        // Game received; initialize game board
        game_state game_board ;
        game_board.Init(buffer) ;

        // If we find a solution to the game, put the results in solution
        move solution[BOARD_SIZE] ;
        int size = 0 ;
        // Search for a solution to the puzzle
        bool found = depthFirstSearch(game_board,size,solution) ;

        // Add solution to stream and return to server.
        if(found) {
            stringstream output;
            output << "found solution = " << endl;
            game_state s;
            s.Init(buffer);
            s.Print(output);
            for (int i = 0; i < size; ++i) {
                s.makeMove(solution[i]);
                output << "-->" << endl;
                s.Print(output);
            }
            output << "solved" << endl;
            string output_str = output.str();
            int count = output_str.size();
            // Convert stream to char array so MPI can handle
            char *buffer = &output_str[0];
            MPI_Isend(buffer, count, MPI_UNSIGNED_CHAR, 0, TAG_SOLUTION, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_SUCCESS);
        }
        // If no solution found, let the server know.
        else {
            MPI_Isend(buffer, 0, MPI_UNSIGNED_CHAR, 0, TAG_NO_SOLUTION, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_SUCCESS);
        }
    }
}


int main(int argc, char *argv[]) {
    // This is a utility routine that installs an alarm to kill off this
    // process if it runs to long.  This will prevent jobs from hanging
    // on the queue keeping others from getting their work done.
    chopsigs_() ;

    // All MPI programs must call this function
    MPI_Init(&argc,&argv) ;

    int rank ;
    int procs ;

    /* Get the number of processors and my processor identification */
    MPI_Comm_size(MPI_COMM_WORLD,&procs) ;
    MPI_Comm_rank(MPI_COMM_WORLD,&rank) ;

    if(rank == 0) {
        // Processor 0 runs the server code
        get_timer() ;// zero the timer
        Server(argc,argv,procs) ;
        // Measure the running time of the server
        cout << "execution time = " << get_timer() << " seconds." << endl ;
    }

    else { Client(); }

    // All MPI programs must call this before exiting
    MPI_Finalize() ;
}
