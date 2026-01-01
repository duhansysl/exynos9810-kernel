#!/usr/bin/env python3
#
# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Sample Usage:
# $ python3 merge_orderfile.py --order-files %../orderfiles/test
#
# Try '-h' for a full list of command line arguments.
#
# Note: We allow three formats: Folder, File, and CSV
# As lower end devices require the most help, you can give
# their order files a higher weight.
# You can only provide weights if you choose File format.
# For example, an order file weight of 4 means the edges
# in the graph will be multiplied by 4.
# CSV and Folder assume all files have a weight of 1.
# An example file can be found at ../test/merge-test/merge.txt

from bitarray import bitarray
import argparse
import graphviz

import orderfile_utils

class Vertex(object):
    """Vertex (symbol) in the graph."""
    def __init__(self, name: str) -> None:
        self.name = name
        self.count = 0

    def __eq__(self, other: object) -> bool:
        if isinstance(other, Vertex):
            return self.name == other.name
        return False

    def __hash__(self) -> int:
        return hash(self.name)

    def __str__(self) -> str:
        return f'{self.name}({self.count})'

    def appears(self) -> None:
        self.count += 1

class Graph(object):
    """Graph representation of the order files."""
    def __init__(self) -> None:
        self.graph = {}
        self.reverse = {}
        self.vertices = {}

    def __str__(self) -> str:
        string = ""
        for (f_symbol, value) in self.graph.items():
            for (t_symbol, weight) in self.graph[f_symbol].items():
                string += f'{f_symbol} --{weight}--> {t_symbol}\n'
        return string

    def addVertex(self, symbol: str) -> None:
        if symbol not in self.vertices:
            v = Vertex(symbol)
            self.vertices[symbol] = v
            self.graph[v] = {}
            self.reverse[v] = {}

        self.vertices[symbol].appears()

    def addEdge(self, from_symbol: str, to_symbol: str) -> None:
        """Add an edge (it represents two symbols are consecutive)."""
        from_vertex = self.vertices.get(from_symbol)
        to_vertex = self.vertices.get(to_symbol)

        if from_vertex is None:
            raise RuntimeError(f"Symbol {from_symbol} is not in graph")

        if to_vertex is None:
            raise RuntimeError(f"Symbol {to_symbol} is not in graph")

        if to_vertex not in self.graph[from_vertex]:
            self.graph[from_vertex][to_vertex] = 0
            self.reverse[to_vertex][from_vertex] = 0

        self.graph[from_vertex][to_vertex] += 1
        self.reverse[to_vertex][from_vertex] += 1

    def removeEdgeCompletely(self, from_symbol: str, to_symbol: str) -> None:
        """Remove the edge from the graph"""
        from_vertex = self.vertices.get(from_symbol)
        to_vertex =  self.vertices.get(to_symbol)

        if from_vertex is None:
            raise RuntimeError(f"Symbol {from_symbol} is not in graph")

        if to_vertex is None:
            raise RuntimeError(f"Symbol {to_symbol} is not in graph")

        del self.graph[from_vertex][to_vertex]
        del self.reverse[to_vertex][from_vertex]

        to_vertex.count -= 1

    def checkVertex(self, symbol: str) -> bool:
        return symbol in self.vertices

    def checkEdge(self, from_symbol: str, to_symbol: str) -> bool:
        if not self.checkVertex(from_symbol):
            return False

        if not self.checkVertex(to_symbol):
            return False

        from_vertex = self.vertices.get(from_symbol)
        to_vertex =  self.vertices.get(to_symbol)

        if from_vertex not in self.graph:
            return False

        return to_vertex in self.graph[from_vertex]

    def checkEdgeWeight(self, from_symbol: str, to_symbol: str, weight: str) -> bool:
        if not self.checkEdge(from_symbol, to_symbol):
            return False

        from_vertex = self.vertices.get(from_symbol)
        to_vertex =  self.vertices.get(to_symbol)

        return self.graph[from_vertex][to_vertex] == weight

    def getOutEdges(self, symbol: str):
        """Graph the out edges for a vertex."""
        out_edges = []
        vertex = self.vertices.get(symbol)
        if vertex is None:
            raise RuntimeError(f"Symbol {symbol} is not in graph")

        for (key, value) in self.graph[vertex].items():
            out_edges.append((key, value))

        return out_edges

    def getInEdges(self, symbol: str):
        """Graph the in edges for a vertex."""
        in_edges = []
        vertex = self.vertices.get(symbol)
        if vertex is None:
            raise RuntimeError(f"Symbol {symbol} is not in graph")

        for (key, value) in self.reverse[vertex].items():
            in_edges.append((key, value))

        return in_edges

    def getRoots(self, reverse=False) -> list[str]:
        """Get the roots of the graph (Vertex with no in edges)."""
        roots = []
        for (symbol,_) in self.vertices.items():
            if not reverse:
                if len(self.getInEdges(symbol)) == 0:
                    roots.append(symbol)
            else:
                # If you want the reverse (vertex with no out edges)
                if len(self.getOutEdges(symbol)) == 0:
                    roots.append(symbol)

        return roots

    def __cyclesUtil(self, vertex: Vertex) -> None:
        self.visited.add(vertex)
        self.curr_search.append(vertex)

        for (out_vertex, _) in self.graph[vertex].items():
            # If vertex already appeared in current depth search, we have a backedge
            if out_vertex in self.curr_search:
                # We save save all vertices in the cycle because an edge from the cycle will be removed
                index = self.curr_search.index(out_vertex)
                temp_lst = self.curr_search[index:]
                self.cycles.append(temp_lst)
            # If vertex visited before in a previous search, we do not need to search from it again
            elif out_vertex not in self.visited:
                self.__cyclesUtil(out_vertex)

        self.curr_search.pop()

    def getCycles(self) -> list[list[tuple[str]]]:
        self.visited = set()
        self.curr_search = []
        self.cycles = []
        lst = []

        for (_, vertex) in self.vertices.items():
            if vertex not in self.visited:
                self.__cyclesUtil(vertex)

        return self.cycles

    # Get immediate dominator for each vertex
    def getDominators(self, post=False):
        # Create a bitarray for each vertex to showcase which vertices
        # are dominators
        num_vertices = len(self.vertices)
        dominators = {}
        mapping = []
        for (_, vertex) in self.vertices.items():
            mapping.append(vertex)
            ba = bitarray(num_vertices)
            ba.setall(True)
            dominators[vertex] = ba

        # Add the root vertices
        stack = []
        roots = self.getRoots(post)
        for root in roots:
            stack.append((None, self.vertices[root]))

        while len(stack) != 0:
            (parent, child) = stack.pop()

            # If no parent, you have no dominators from above
            # If you have a parent, your dominations is the common dominators
            # between all parents
            if parent is None:
                dominators[child].setall(False)
            else:
                dominators[child] &= dominators[parent]

            # You are dominator of yourself
            index = mapping.index(child)
            dominators[child][index] = True
            if not post:
                for (out_vertex,_) in self.graph[child].items():
                    stack.append((child, out_vertex))
            else:
                for (out_vertex,_) in self.reverse[child].items():
                    stack.append((child, out_vertex))

        for (vertex, ba) in dominators.items():
            # If no Trues in bitarray, you have no immediate dominator
            # because you are a root vertex. Else, you can find the
            # most left True vertex excluding yourself
            index = mapping.index(vertex)
            ba[index] = False
            if True not in ba:
                dominators[vertex] = None
            else:
                # Due to reverse, this is the actual index in the initial bitarray
                dominator_index = ba.index(True)
                dominators[vertex] = mapping[dominator_index]

        return dominators

    def __printOrderUtil(self, vertex):
        # If already visit, we do not need to get order
        if vertex in self.visited:
            return

        self.order.append(vertex)
        self.visited.add(vertex)

        # Get out edges and sort them based on their weightage
        out_edges = self.getOutEdges(vertex.name)
        out_edges.sort(key = lambda x: x[1], reverse=True)

        # We continue dfs based on the largest weight
        for (out, _) in out_edges:
            self.__printOrderUtil(out)

    def printOrder(self, output):
        self.order = []
        self.visited = set()
        stack = []

        # Create an order using DFS from the root
        for root in self.getRoots():
            self.__printOrderUtil(self.vertices[root])

        # Write the order to a file
        with open(output, "w") as f:
            for vertex in self.order:
                f.write(f"{vertex.name}\n")

    def exportGraph(self, output: str) -> None:
        """Export graph as a dot file and pdf file."""
        dot = graphviz.Digraph(comment='Graph Representation of Orderfile')

        for (from_vertex, to_vertices) in self.graph.items():
            for (to_vertex, weight) in to_vertices.items():
                dot.edge(from_vertex.__str__(), to_vertex.__str__(), label=str(weight))

        dot.render(filename=output)


def parse_args() -> argparse.Namespace:
    """Parses and returns command line arguments."""

    parser = argparse.ArgumentParser(prog="merge_orderfile",
                                    description="Merge Order Files")

    parser.add_argument(
        "--order-files",
        required=True,
        help="A collection of order files that need to be merged together."
             "Format: A file-per-line file with @, a folder with ^, or comma separated values within a quotation."
             "For example, you can say @file.txt, ^path/to/folder or '1.orderfile,2.orderfile'.")

    parser.add_argument(
        "--output",
        default="default.orderfile",
        help="Provide the output file name for the order file. Default Name: default.orderfile")

    parser.add_argument(
        "--graph-image",
        help="Provide the output image name for the graph representation of the order files.")

    return parser.parse_args()

def removeCycles(graph: Graph) -> None:
    # Remove cycles created by combining order files
    for cycleList in graph.getCycles():
        # Get the sum of in edge weights for all vertices in the cycle
        # We exclude the cycle edges from the calculation
        # For example, cycle = [a,b,c] where cycle_edges=[a->b, b->c, c->a]
        # in_edges(a) = [main, c]
        # in_edges(b) = [a]
        # in_edges(c) = [b]
        #
        # Excluding cycle edges:
        # in_edges(a) = [main] = 1
        # in_edges(b) = [] = 0
        # in_edges(c) = [] = 0
        inner_edges = [graph.getInEdges(vertex.name) for vertex in cycleList]
        inner_weights = []
        for inner_edge in inner_edges:
            total = 0
            for edge in inner_edge:
                if edge[0] not in cycleList:
                    total += edge[1]
            inner_weights.append(total)

        # We remove the cycle edge that leads to the highest sum of in-edges for a vertex
        # because the vertex has other options for ordering.
        # In the above example, we remove c->a
        max_inner_weight = max(inner_weights)
        index = inner_weights.index(max_inner_weight)
        prev = index - 1
        if prev < 0:
            prev = len(inner_weights) - 1
        to_vertex = cycleList[index]
        from_vertex = cycleList[prev]

        graph.removeEdgeCompletely(from_vertex.name, to_vertex.name)

def addSymbolsToGraph(graph: Graph, order: list[str], weight: int = 1) -> None:
    prev_symbol = None
    for symbol in order:
        graph.addVertex(symbol)

        if prev_symbol is not None:
            for i in range(weight):
                graph.addEdge(prev_symbol, symbol)

        prev_symbol = symbol

def createGraph(files: list[str]) -> Graph:
    graph = Graph()

    # Create graph representation based on combining the order files
    for (orderfile, weight) in files:
        with open(orderfile, "r", encoding="utf-8") as f:
            lst = []
            for line in f:
                line = line.strip()
                lst.append(line)

            addSymbolsToGraph(graph, lst, weight)

    return graph

def main() -> None:
    args = parse_args()

    files = orderfile_utils.parse_merge_list(args.order_files)
    graph = createGraph(files)

    # Assert no cycles after removing them
    removeCycles(graph)
    assert(len(graph.getCycles()) == 0)

    # Create an image of the graph representation
    if args.graph_image:
        graph.exportGraph(args.graph_image)

    # Create order file from the graph structure
    graph.printOrder(args.output)

if __name__ == '__main__':
    main()
