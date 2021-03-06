// Copyright (c) 2014, Sailing Lab
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the <ORGANIZATION> nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
#include <boost/shared_array.hpp>
#include <boost/program_options.hpp>
namespace boost_po = boost::program_options;

#include <string>
#include <comm_handler.hpp>
#include <stdio.h>
#include <glog/logging.h>

using namespace petuum;

#define PUB_ID_ST 500

int main(int argc, char *argv[]){
  boost_po::options_description options("Allowed options");
  std::string ip;
  std::string port;
  int32_t id;
  int num_clients;
  int32_t *clients;
  std::string pubport;
 
  options.add_options()
    ("id", boost_po::value<int32_t>(&id)->default_value(0), "node id")
    ("ip", boost_po::value<std::string>(&ip)->default_value("127.0.0.1"), "ip address")
    ("port", boost_po::value<std::string>(&port)->default_value("9999"), "port number")
    ("ncli", boost_po::value<int>(&num_clients)->default_value(1), "number of clients expected")
    ("pubport", boost_po::value<std::string>(&pubport)->default_value("10000"), "publish port number");
  
  boost_po::variables_map options_map;
  boost_po::store(boost_po::parse_command_line(argc, argv, options), options_map);
  boost_po::notify(options_map);
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "server started at " << ip << ":" << port;

  try{
    clients = new int32_t[num_clients];
  }catch(std::bad_alloc &e){
    LOG(ERROR) << "failed to allocate memory for client array";
    return -1;
  }
  
  ConfigParam config(id, true, ip, port);
  
  CommHandler *comm;
  try{
    comm = new CommHandler(config);
  }catch(std::bad_alloc &e){
    LOG(ERROR) << "failed to create comm";
    return -1;
  }

  LOG(INFO) << "comm_handler created";

  zmq::context_t zmq_ctx(1);

  int suc = comm->Init(&zmq_ctx);
  if(suc == 0) LOG(INFO) << "comm_handler init succeeded";
  else{
    LOG(ERROR) << "comm_handler init failed";
    return -1;
  }
  LOG(INFO) << "start waiting for " << num_clients << " connections";

  int i;
  for(i = 0; i < num_clients; i++){
    int32_t cid;
    cid = comm->GetOneConnection();
    if(cid < 0){
      LOG(ERROR) << "wait for connection failed";
      return -1;
    }
    LOG(INFO) << "received connection from " << cid;
    clients[i] = cid;
  }

  LOG(INFO) << "received expected number of clients, calling init_pub";

  suc = comm->InitPub(ip, pubport, PUB_ID_ST, num_clients);
  if(suc == 0){
    LOG(INFO) << "publisher got all subscribers";
  }else{
    LOG(INFO) << "init_pub failed";
    exit(1);
  }
  for(i = 0; i < num_clients; ++i){
    suc = comm->Send(clients[i], (uint8_t *) (clients + i), sizeof(int32_t));
    
    LOG(INFO) << "send task to " << clients[i];
    assert(suc == sizeof(int32_t));
  }
  
  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(INFO) << "UNICAST SEND/RECV WORKS!!";
  LOG(INFO) << "START TESTING PUBLISH!!";

  suc = comm->Publish(PUB_ID_ST, (uint8_t *) "HEREhello", 10);

  assert(suc == 10);
  LOG(INFO) << "MULTICAST ENDS!!";

  for(i = 0; i < num_clients; ++i){
    boost::shared_array<uint8_t> data; //I'm expecting a string
    int32_t cid;
    suc = comm->Recv(cid, data);
    assert(suc > 0);
    printf("Received msg : %s from %d\n", (char *) data.get(), cid);
  }

  LOG(INFO) << "TEST NEARLY PASSED!! SHUTTING DOWN COMMTHREAD!!";
  suc = comm->ShutDown();
  if(suc < 0) LOG(ERROR) << "failed to shut down comm handler";

  delete comm;
  LOG(INFO) << "TEST PASSED!! EXITING!!";
  return 0;
}
