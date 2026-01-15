//===------------------------------------------------------------*- C++ -*-===//
//
//                                     SHAD
//
//      The Scalable High-performance Algorithms and Data Structure Library
//
//===----------------------------------------------------------------------===//
//
// Copyright 2018 Battelle Memorial Institute
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
//
//===----------------------------------------------------------------------===//

// Simple implementation of triangle counting, through graph pattern matching

#include <atomic>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <likwid.h>
#include "shad/core/algorithm.h"
#include "shad/data_structures/array.h"
#include "shad/extensions/graph_library/algorithms/jp_coloring.h"
#include "shad/extensions/graph_library/edge_index.h"
#include "shad/runtime/runtime.h"
#include "shad/util/measure.h"

using graph_t = shad::EdgeIndex<size_t, size_t>;

// The GraphReader expects an input file in METIS dump format
graph_t::ObjectID GraphReader(std::ifstream &GFS) {
  std::string line;
  unsigned long EdgeNumber, VertexNumber;

  std::getline(GFS, line);

  std::istringstream headlineStream(line);

  headlineStream >> VertexNumber >> EdgeNumber;
  EdgeNumber <<= 1;

  auto eiGraph = graph_t::Create(VertexNumber);
  shad::rt::Handle handle;

  for (size_t i = 0L; i < VertexNumber; i++) {
    size_t destination;

    std::getline(GFS, line);
    std::istringstream lineStream(line);
    std::vector<size_t> edges;
    while (!lineStream.eof()) {
      lineStream >> destination;
      destination--;
      edges.push_back(destination);
    }
    eiGraph->AsyncInsertEdgeList(handle, i, edges.data(), edges.size());
  }
  shad::rt::waitForCompletion(handle);
  return eiGraph->GetGlobalID();
}

void printHelp(const std::string programName) {
  std::cerr << "Usage: " << programName << " FILENAME" << std::endl;
}

namespace shad {

int main(int argc, char **argv) {
  if (argc != 2) {
    printHelp(argv[0]);
    return -1;
  }

  graph_t::ObjectID OID(-1);
  auto loadingTime = shad::measure<std::chrono::seconds>::duration([&]() {
    // The GraphReader expects an input file in METIS dump format
    std::ifstream inputFile;
    inputFile.open(argv[1], std::ifstream::in);
    OID = GraphReader(inputFile);
  });
  std::cout << "Graph loaded in " << loadingTime.count()
            << " seconds\nLet's start..." << std::endl;
  auto eiPtr = graph_t::GetPtr(OID);

  size_t num_vertices = eiPtr->Size();
  std::cout << "NumVertices: " << num_vertices
            << " Num Edges: " << eiPtr->NumEdges() << std::endl;
  shad::Array<int32_t>::SharedPtr result;
  LIKWID_MARKER_REGISTER("foo");
  LIKWID_MARKER_START("foo");
  auto duration = shad::measure<std::chrono::seconds>::duration(
      [&]() { result = jp_coloring<graph_t, size_t>(OID, num_vertices); });
  LIKWID_MARKER_STOP("foo");
  if (shad::rt::thisLocality() == shad::rt::Locality(0)) {
    for (auto i = 0; i < num_vertices; ++i) {
      std::cout << i << " " << result->At(i) << "\n";
    }

    auto c =
        *(shad::max_element(result->begin(), result->end(), std::less<>())) + 1;
    std::cout << "Num colors " << c << "\n";
  }

  std::cout << "Success " << duration.count() << " seconds" << std::endl;

  graph_t::Destroy(OID);
  shad::Array<int32_t>::Destroy(result->GetGlobalID());
  return 0;
}

}  // namespace shad
