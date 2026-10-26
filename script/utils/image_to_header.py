import sys
from PIL import Image
import os

def convert_image_to_header(image_path, header_name="image_data.h", array_name="input_image"):
    """
    Converts an image to a grayscale C header file.
    """
    try:
        # Open image and convert to grayscale ('L' mode is 8-bit pixels, 0-255)
        img = Image.open(image_path).convert('L')
        width, height = img.size
        
        # Get raw pixel data
        pixels = list(img.getdata())
        
        with open(header_name, 'w') as f:
            f.write(f"/* Auto-generated from {os.path.basename(image_path)} */\n")
            f.write(f"#ifndef {header_name.replace('.', '_').upper()}\n")
            f.write(f"#define {header_name.replace('.', '_').upper()}\n\n")
            
            f.write(f"#define IMG_WIDTH  {width}\n")
            f.write(f"#define IMG_HEIGHT {height}\n\n")
            
            f.write(f"const unsigned char {array_name}[IMG_HEIGHT][IMG_WIDTH] = {{\n")
            
            for y in range(height):
                f.write("    {")
                # Slice the flat pixel list for the current row
                row_pixels = pixels[y * width : (y + 1) * width]
                # Convert pixels to comma-separated strings
                f.write(", ".join(map(str, row_pixels)))
                
                if y == height - 1:
                    f.write("}\n")
                else:
                    f.write("},\n")
            
            f.write("};\n\n")
            f.write("#endif\n")
            
        print(f"Successfully created {header_name} ({width}x{height})")
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    # Check if an image path was provided
    if len(sys.argv) < 2:
        print("Usage: python img_to_header.py <path_to_image>")
    else:
        convert_image_to_header(sys.argv[1])