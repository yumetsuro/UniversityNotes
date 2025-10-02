import networkx as nx
import matplotlib.pyplot as plt
import numpy as np
import random

def generate_neuron(n_branches=60, branching_prob=0.3):
    """
    Generate a random tree graph to mimic a neuron dendritic tree.
    n_branches: number of possible nodes
    branching_prob: probability that a node branches further
    """
    G = nx.Graph()
    G.add_node(0)  # soma
    frontier = [0]
    node_id = 1
    
    while node_id < n_branches and frontier:
        parent = random.choice(frontier)
        frontier.remove(parent)
        
        # each node can branch 1-3 times
        # normalize before sampling
        norm = [1-branching_prob, 0.3, 0.4, 0.3]
        norm = [x/sum(norm) for x in norm]
        n_children = np.random.choice([0,1,2,3], p=norm)
        #n_children = np.random.choice([0,1,2,3], p=[1-branching_prob, 0.3, 0.4, 0.3])
        
        for _ in range(n_children):
            if node_id >= n_branches:
                break
            G.add_edge(parent, node_id)
            frontier.append(node_id)
            node_id += 1
    return G

def plot_neuron(G):
    # spring layout gives "organic" positions
    pos = nx.spring_layout(G, iterations=100, seed=42)
    
    plt.figure(figsize=(6,8))
    nx.draw(
        G, pos,
        node_size=0,  # hide nodes
        width=1.5,
        edge_color="blue"
    )
    
    # highlight a random branch in red
    path = nx.shortest_path(G, source=0, target=max(G.nodes))
    nx.draw_networkx_edges(
        G, pos,
        edgelist=[(path[i], path[i+1]) for i in range(len(path)-1)],
        width=2.5,
        edge_color="red"
    )
    
    plt.axis("off")
    plt.show()

# Generate and plot
G = generate_neuron(n_branches=80, branching_prob=0.5)
plot_neuron(G)
