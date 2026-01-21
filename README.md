# QuizShow
A C++ app which utilizes WIN32API and mongo-cxx-driver to create a server-client model using sockets to simulate a host addressing questions to multiple contestants in a competition.

## How to use
The server must be started first. Every instance of the client will connect to the server and receive questions one by one (and receive points for answering correctly) until either the server ends the game or all the questions in the cathegory are exhausted.

## Technologies
MongoDB - for storing the questions

WIN32API - for establishing the connection server/client
