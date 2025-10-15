import networkx as nx
import matplotlib.pyplot as plt
import numpy as np
import random
from matplotlib.colors import LinearSegmentedColormap

def generate_neuron(n_branches=60, branching_prob=0.3, angle_constraint=45):
    """
    Generate a random tree graph to mimic a neuron dendritic tree.
    n_branches: number of possible nodes
    branching_prob: probability that a node branches further
    angle_constraint: maximum angle deviation from upward direction (degrees)
    """
    G = nx.Graph()
    G.add_node(0)  # soma at origin
    
    # Store positions and angles for each node
    positions = {0: np.array([0.0, 0.0])}
    angles = {0: 90}  # start pointing upward (90 degrees)
    depths = {0: 0}   # distance from soma
    
    frontier = [0]
    node_id = 1
    
    while node_id < n_branches and frontier:
        parent = random.choice(frontier)
        frontier.remove(parent)
        
        # Determine number of children - simpler logic
        # Higher branching_prob means more branching
        if np.random.random() < branching_prob:
            n_children = np.random.choice([1, 2, 3], p=[0.3, 0.5, 0.2])
        else:
            n_children = np.random.choice([0, 1], p=[0.3, 0.7])
        
        parent_pos = positions[parent]
        parent_angle = angles[parent]
        
        for i in range(n_children):
            if node_id >= n_branches:
                break
                
            # Generate new angle within constraint - free angles
            angle_deviation = np.random.uniform(-angle_constraint, angle_constraint)
            new_angle = parent_angle + angle_deviation
            
            # Ensure general upward growth (bias toward positive y)
            if new_angle < 30:
                new_angle = 30 + np.random.uniform(0, 20)
            elif new_angle > 150:
                new_angle = 150 - np.random.uniform(0, 20)
            
            # Calculate new position
            branch_length = np.random.uniform(0.3, 0.8)
            angle_rad = np.radians(new_angle)
            
            new_pos = parent_pos + branch_length * np.array([
                np.cos(angle_rad),
                np.sin(angle_rad)
            ])
            
            G.add_edge(parent, node_id)
            positions[node_id] = new_pos
            angles[node_id] = new_angle
            depths[node_id] = depths[parent] + branch_length
            
            # Add to frontier - most nodes continue to grow
            if np.random.random() < 0.8:  # 80% chance to continue growing
                frontier.append(node_id)
                
            node_id += 1
    
    return G, positions, depths

def smooth_path(pos1, pos2, parent_node, child_node, G, positions):
    """Create a smooth Bezier curve between two points using neighboring context"""
    # Get parent's parent if it exists for smoother entry angle
    parent_parents = list(G.neighbors(parent_node))
    if len(parent_parents) > 0 and parent_node != 0:
        # Find the actual parent (lower node id)
        grandparent = min([n for n in parent_parents if n < parent_node], default=None)
        if grandparent is not None:
            # Use direction from grandparent to create smooth curve
            prev_direction = pos1 - positions[grandparent]
            control1 = pos1 + prev_direction * 0.3
        else:
            control1 = pos1 + (pos2 - pos1) * 0.3
    else:
        control1 = pos1 + (pos2 - pos1) * 0.3
    
    # Second control point
    control2 = pos2 - (pos2 - pos1) * 0.3
    
    # Generate cubic Bezier curve
    t = np.linspace(0, 1, 30)
    curve_x = (1-t)**3 * pos1[0] + 3*(1-t)**2*t * control1[0] + 3*(1-t)*t**2 * control2[0] + t**3 * pos2[0]
    curve_y = (1-t)**3 * pos1[1] + 3*(1-t)**2*t * control1[1] + 3*(1-t)*t**2 * control2[1] + t**3 * pos2[1]
    
    return curve_x, curve_y

def plot_neuron(G, positions, depths):
    plt.figure(figsize=(8, 10))
    
    # Create a colormap from orange/red (soma) to blue (extremes)
    colors = ['#FF6B35', '#F7931E', '#FFD23F', '#06FFA5', '#4ECDC4', '#45B7D1', '#96CEB4']
    n_colors = len(colors)
    cmap = LinearSegmentedColormap.from_list("neuron", colors, N=256)
    
    # Normalize depths for coloring
    max_depth = max(depths.values()) if depths.values() else 1
    if max_depth == 0:
        max_depth = 1  # avoid division by zero
    normalized_depths = {node: depth/max_depth for node, depth in depths.items()}
    
    # Draw edges with smooth curves and gradient coloring
    for edge in G.edges():
        node1, node2 = edge
        # Ensure node1 is parent (lower id)
        if node1 > node2:
            node1, node2 = node2, node1
            
        pos1 = positions[node1]
        pos2 = positions[node2]
        
        # Use the average depth for edge color
        avg_depth = (normalized_depths[node1] + normalized_depths[node2]) / 2
        color = cmap(avg_depth)
        
        # Draw smooth curve
        curve_x, curve_y = smooth_path(pos1, pos2, node1, node2, G, positions)
        plt.plot(curve_x, curve_y, color=color, linewidth=2.5, alpha=0.8)
    
    # Draw biologically plausible soma (cell body)
    soma_pos = positions[0]
    
    # Create an irregular, organic-looking soma
    theta = np.linspace(0, 2*np.pi, 100)
    # Add some irregularity to make it look more organic
    radius_variation = 0.15 + 0.05 * np.sin(5*theta) + 0.03 * np.cos(7*theta)
    soma_x = soma_pos[0] + radius_variation * np.cos(theta)
    soma_y = soma_pos[1] + radius_variation * np.sin(theta)
    
    # Fill the soma with a gradient-like appearance
    plt.fill(soma_x, soma_y, color='#FF6B35', alpha=0.9, zorder=5, edgecolor='#FF4500', linewidth=2)
    
    # Add a slightly lighter inner circle to simulate depth
    inner_radius = radius_variation * 0.5
    inner_x = soma_pos[0] + inner_radius * np.cos(theta)
    inner_y = soma_pos[1] + inner_radius * np.sin(theta)
    plt.fill(inner_x, inner_y, color='#FF8C55', alpha=0.7, zorder=6)
    
    plt.axis("equal")
    plt.axis("off")
    plt.tight_layout()
    
    # Set background to black for better contrast
    plt.gca().set_facecolor('black')
    plt.gcf().patch.set_facecolor('black')
    
    # Save as SVG (before showing to preserve the figure)
    plt.savefig('neuron_stylized.svg', format='svg', dpi=300, bbox_inches='tight', 
                pad_inches=0, facecolor='black', edgecolor='none')
    print("Neuron saved as neuron_stylized.svg")
    plt.show()
    # sv

# Generate and plot
random.seed(42)  # for reproducible results
np.random.seed(42)

# then save as svg

G, positions, depths = generate_neuron(n_branches=300, branching_prob=0.69, angle_constraint=40)
plot_neuron(G, positions, depths)