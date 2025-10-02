import svgwrite
from svgwrite import cm, mm

def create_equivalent_circuit():
    """
    Creates an SVG representation of an equivalent circuit with:
    - AC voltage source
    - Capacitor
    - Three parallel branches with variable resistors and voltage sources
    - Ground connections
    """
    
    # Create SVG drawing
    dwg = svgwrite.Drawing('equivalent_circuit.svg', size=('800px', '400px'), profile='full')
    dwg.viewbox(0, 0, 800, 400)
    
    # Define colors
    bg_color = 'black'
    line_color = 'white'
    text_color = 'white'
    
    # Add black background
    dwg.add(dwg.rect(insert=(0, 0), size=('100%', '100%'), fill=bg_color))
    
    # Define positions
    left_margin = 100
    top_y = 100
    bottom_y = 300
    middle_y = (top_y + bottom_y) / 2
    
    # AC Source position
    ac_x = left_margin
    ac_y = middle_y
    
    # Capacitor position
    cap_x = 220
    
    # Three branches starting position
    branch_start_x = 320
    branch_spacing = 120
    
    # Helper function to draw sine wave for AC source
    def draw_ac_source(x, y, size=30):
        # Circle for voltage source
        dwg.add(dwg.circle(center=(x, y), r=size, 
                          fill='none', stroke=line_color, stroke_width=2))
        
        # Sine wave inside (simplified)
        wave_path = f"M {x-20},{y} Q {x-10},{y-15} {x},{y} Q {x+10},{y+15} {x+20},{y}"
        dwg.add(dwg.path(d=wave_path, fill='none', stroke=line_color, stroke_width=2))
        
        return size
    
    # Helper function to draw capacitor
    def draw_capacitor(x, y_top, y_bottom):
        plate_length = 40
        gap = 10
        middle = (y_top + y_bottom) / 2
        
        # Top plate
        dwg.add(dwg.line(start=(x - plate_length/2, middle - gap/2), 
                        end=(x + plate_length/2, middle - gap/2),
                        stroke=line_color, stroke_width=3))
        # Bottom plate
        dwg.add(dwg.line(start=(x - plate_length/2, middle + gap/2), 
                        end=(x + plate_length/2, middle + gap/2),
                        stroke=line_color, stroke_width=3))
        
        # Connection lines
        dwg.add(dwg.line(start=(x, y_top), end=(x, middle - gap/2),
                        stroke=line_color, stroke_width=2))
        dwg.add(dwg.line(start=(x, middle + gap/2), end=(x, y_bottom),
                        stroke=line_color, stroke_width=2))
    
    # Helper function to draw variable resistor
    def draw_variable_resistor(x, y1, y2, label):
        rect_height = 50
        rect_width = 25
        middle_y = (y1 + y2) / 2
        
        # Rectangle for resistor
        dwg.add(dwg.rect(insert=(x - rect_width/2, middle_y - rect_height/2),
                        size=(rect_width, rect_height),
                        fill='none', stroke=line_color, stroke_width=2))
        
        # Arrow for variable resistor
        arrow_start_x = x - rect_width/2 - 15
        arrow_start_y = middle_y + rect_height/2 + 10
        arrow_end_x = x + rect_width/2 + 5
        arrow_end_y = middle_y - rect_height/2 - 10
        
        dwg.add(dwg.line(start=(arrow_start_x, arrow_start_y),
                        end=(arrow_end_x, arrow_end_y),
                        stroke=line_color, stroke_width=2))
        
        # Arrowhead
        arrow_size = 8
        dwg.add(dwg.polygon(points=[
            (arrow_end_x, arrow_end_y),
            (arrow_end_x - arrow_size, arrow_end_y + arrow_size/2),
            (arrow_end_x - arrow_size/2, arrow_end_y + arrow_size)
        ], fill=line_color, stroke=line_color))
        
        # Label
        dwg.add(dwg.text(label, insert=(x + rect_width/2 + 15, middle_y + 5),
                        fill=text_color, font_size='20px', font_family='Arial'))
        
        # Connection lines
        dwg.add(dwg.line(start=(x, y1), end=(x, middle_y - rect_height/2),
                        stroke=line_color, stroke_width=2))
        dwg.add(dwg.line(start=(x, middle_y + rect_height/2), end=(x, y2),
                        stroke=line_color, stroke_width=2))
    
    # Helper function to draw voltage source (battery)
    def draw_battery(x, y1, y2, label):
        gap = 10
        middle = (y1 + y2) / 2
        
        # Positive terminal (longer line)
        dwg.add(dwg.line(start=(x - 20, middle - gap), end=(x + 20, middle - gap),
                        stroke=line_color, stroke_width=3))
        
        # Negative terminal (shorter line)
        dwg.add(dwg.line(start=(x - 12, middle + gap), end=(x + 12, middle + gap),
                        stroke=line_color, stroke_width=3))
        
        # Connection lines
        dwg.add(dwg.line(start=(x, y1), end=(x, middle - gap),
                        stroke=line_color, stroke_width=2))
        dwg.add(dwg.line(start=(x, middle + gap), end=(x, y2),
                        stroke=line_color, stroke_width=2))
        
        # Label
        dwg.add(dwg.text(label, insert=(x + 25, middle + 5),
                        fill=text_color, font_size='20px', font_family='Arial'))
    
    # Helper function to draw ground
    def draw_ground(x, y):
        # Three horizontal lines decreasing in length
        dwg.add(dwg.line(start=(x - 20, y), end=(x + 20, y),
                        stroke=line_color, stroke_width=3))
        dwg.add(dwg.line(start=(x - 13, y + 7), end=(x + 13, y + 7),
                        stroke=line_color, stroke_width=3))
        dwg.add(dwg.line(start=(x - 7, y + 14), end=(x + 7, y + 14),
                        stroke=line_color, stroke_width=3))
    
    # Draw AC voltage source
    radius = draw_ac_source(ac_x, ac_y, 30)
    
    # Line from AC source to top
    dwg.add(dwg.line(start=(ac_x, ac_y - radius), end=(ac_x, top_y),
                    stroke=line_color, stroke_width=2))
    
    # Line from AC source to bottom (ground)
    dwg.add(dwg.line(start=(ac_x, ac_y + radius), end=(ac_x, bottom_y),
                    stroke=line_color, stroke_width=2))
    
    # Ground at AC source
    draw_ground(ac_x, bottom_y)
    
    # Vm(t) label above AC source
    dwg.add(dwg.text('Vₘ(t)', insert=(ac_x - 25, ac_y - radius - 15),
                    fill=text_color, font_size='22px', font_family='Arial', font_style='italic'))
    
    # Top horizontal line from AC to capacitor
    dwg.add(dwg.line(start=(ac_x, top_y), end=(cap_x, top_y),
                    stroke=line_color, stroke_width=2))
    
    # Draw capacitor
    draw_capacitor(cap_x, top_y, bottom_y)
    
    # Cm label
    dwg.add(dwg.text('Cₘ', insert=(cap_x - 35, middle_y + 5),
                    fill=text_color, font_size='20px', font_family='Arial', font_style='italic'))
    
    # Horizontal line from capacitor to branches
    dwg.add(dwg.line(start=(cap_x, top_y), end=(branch_start_x + 2 * branch_spacing, top_y),
                    stroke=line_color, stroke_width=2))
    
    # Bottom horizontal line for ground
    dwg.add(dwg.line(start=(cap_x, bottom_y), end=(branch_start_x + 2 * branch_spacing, bottom_y),
                    stroke=line_color, stroke_width=2))
    
    # Draw three branches
    branches = [
        {'label_g': 'gₙₐ', 'label_e': 'Eₙₐ'},
        {'label_g': 'gₖ', 'label_e': 'Eₖ'},
        {'label_g': 'gᴄₗ', 'label_e': 'Eᴄₗ'}
    ]
    
    for i, branch in enumerate(branches):
        x = branch_start_x + i * branch_spacing
        
        # Vertical line from top
        dwg.add(dwg.line(start=(x, top_y), end=(x, top_y + 20),
                        stroke=line_color, stroke_width=2))
        
        # Draw variable resistor
        resistor_top = top_y + 20
        resistor_bottom = middle_y - 30
        draw_variable_resistor(x, resistor_top, resistor_bottom, branch['label_g'])
        
        # Draw battery
        battery_top = middle_y + 30
        battery_bottom = bottom_y - 20
        draw_battery(x, battery_top, battery_bottom, branch['label_e'])
        
        # Line connecting resistor to battery
        dwg.add(dwg.line(start=(x, resistor_bottom), end=(x, battery_top),
                        stroke=line_color, stroke_width=2))
        
        # Line from battery to ground
        dwg.add(dwg.line(start=(x, battery_bottom), end=(x, bottom_y),
                        stroke=line_color, stroke_width=2))
    
    # Ground symbol at the bottom
    ground_x = branch_start_x + branch_spacing
    draw_ground(ground_x, bottom_y)
    
    return dwg

if __name__ == '__main__':
    drawing = create_equivalent_circuit()
    drawing.save()
    print("Equivalent circuit SVG has been generated: equivalent_circuit.svg")
