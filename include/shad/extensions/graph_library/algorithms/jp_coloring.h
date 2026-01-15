#ifndef INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_JP_COVER_H_
#define INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_JP_COVER_H_

#include <cstdint>
#include <limits>

#include "shad/data_structures/array.h"
#include "shad/extensions/graph_library/edge_index.h"
#include "shad/runtime/runtime.h"

// -------------------------
// Helper: deterministic-ish hash for priority (no RNG needed)
// -------------------------
static inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

// Neighbor scan to find if any UNCOLORED neighbor beats v in priority.
template <typename VertexT>
void jp_check_neighbor(shad::rt::Handle & /*handle*/, const VertexT &v,
                       const VertexT &nbr,
                       shad::Array<int32_t>::ObjectID &colorID,
                       shad::Array<uint8_t>::ObjectID &isWinnerID) {
  auto color = shad::Array<int32_t>::GetPtr(colorID);
  if (color->At(v) >= 0) return;    // v already colored
  if (color->At(nbr) >= 0) return;  // only compare vs uncolored neighbors

  const uint32_t pv = hash32(static_cast<uint32_t>(v));
  const uint32_t pn = hash32(static_cast<uint32_t>(nbr));

  // Tie-break by vertex id to be deterministic
  if (pn > pv || (pn == pv && nbr > v)) {
    auto win = shad::Array<uint8_t>::GetPtr(isWinnerID);
    win->InsertAt(v, 0);  // v is NOT a winner
  }
}

// For each vertex: tentatively mark winner=1, then scan neighbors to
// disqualify.
template <typename GraphT, typename VertexT>
void jp_pick_winners(shad::rt::Handle &handle, const VertexT &v,
                     typename GraphT::ObjectID &gid,
                     shad::Array<int32_t>::ObjectID &colorID,
                     shad::Array<uint8_t>::ObjectID &isWinnerID) {
  auto color = shad::Array<int32_t>::GetPtr(colorID);
  if (color->At(v) >= 0) return;

  auto win = shad::Array<uint8_t>::GetPtr(isWinnerID);
  win->InsertAt(v, 1);  // tentatively winner

  auto g = GraphT::GetPtr(gid);
  g->AsyncForEachNeighbor(handle, v, jp_check_neighbor<VertexT>, colorID,
                          isWinnerID);
}

struct Scratch {
  std::vector<uint32_t> seen;
};

// A simple (slow but correct) color choice: scan neighbors, mark forbidden
// colors in a small local bitmap. You can optimize later (see section 6).
template <typename GraphT, typename VertexT>
void jp_color_winner_neighbor(const VertexT &v, const VertexT &nbr,
                              shad::Array<int32_t>::ObjectID &colorID,
                              Scratch *&sp) {
  auto color = shad::Array<int32_t>::GetPtr(colorID);
  int32_t cn = color->At(nbr);
  if (cn >= 0) sp->seen.push_back(cn);
}

template <typename GraphT, typename VertexT>
void jp_color_winners(shad::rt::Handle &handle, const VertexT &v,
                      typename GraphT::ObjectID &gid,
                      shad::Array<int32_t>::ObjectID &colorID,
                      shad::Array<uint8_t>::ObjectID &isWinnerID) {
  auto color = shad::Array<int32_t>::GetPtr(colorID);
  if (color->At(v) >= 0) return;

  auto win = shad::Array<uint8_t>::GetPtr(isWinnerID);
  if (win->At(v) == 0) return;  // not a winner this round

  Scratch s;
  Scratch *sp = &s;

  auto g = GraphT::GetPtr(gid);
  // TODO does not work with multi node, potentially a vertex could be on
  // another node
  g->ForEachNeighbor(v, jp_color_winner_neighbor<GraphT, VertexT>, colorID, sp);

  auto &u = s.seen;
  std::sort(u.begin(), u.end());
  u.erase(std::unique(u.begin(), u.end()), u.end());

  int32_t mex = 0;
  for (int32_t c : u) {
    if (c == mex)
      ++mex;
    else if (c > mex)
      break;
  }
  color->InsertAt(v, mex);
  u.clear();
}

// Public API: returns vertex->color (clique id) on conflict graph => clique
// cover on compatibility graph.
template <typename GraphT, typename VertexT>
shad::Array<int32_t>::SharedPtr jp_coloring(typename GraphT::ObjectID gid,
                                            size_t num_vertices) {
  auto color = shad::Array<int32_t>::Create(num_vertices, -1);
  auto win = shad::Array<uint8_t>::Create(num_vertices, 0);

  auto colorID = color->GetGlobalID();
  auto winID = win->GetGlobalID();

  auto g = GraphT::GetPtr(gid);

  // Very simple termination: loop fixed rounds until no -1 remains would
  // require a reduction; start with a conservative upper bound for a first
  // version.
  const size_t max_rounds = num_vertices;  // refine later
  for (size_t r = 0; r < max_rounds; ++r) {
    shad::rt::Handle h1;
    g->AsyncForEachVertex(h1, jp_pick_winners<GraphT, VertexT>, gid, colorID,
                          winID);
    shad::rt::waitForCompletion(h1);

    shad::rt::Handle h2;
    g->AsyncForEachVertex(h2, jp_color_winners<GraphT, VertexT>, gid, colorID,
                          winID);
    shad::rt::waitForCompletion(h2);
  }

  shad::Array<uint8_t>::Destroy(win->GetGlobalID());

  return color;
}

#endif  // INCLUDE_SHAD_EXTENSIONS_GRAPH_LIBRARY_ALGORITHMS_JP_COVER_H_
