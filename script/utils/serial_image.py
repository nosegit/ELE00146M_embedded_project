import serial
import numpy as np
from PIL import Image
import struct
import sys

# Configuration
SERIAL_PORT = 'COM6'
BAUD_RATE = 115200

def print_progress_bar(current, total, bar_length=60):
    """Prints a simple progress bar to the console."""
    fraction = current / total
    arrow = int(fraction * bar_length - 1) * '=' + '>'
    padding = int(bar_length - len(arrow)) * ' '
    ending = '\n' if current >= total else '\r'
    
    percent = fraction * 100
    print(f"[{arrow}{padding}] {percent:.1f}% ({current}/{total} bytes)", end=ending)
    sys.stdout.flush()

def capture_raw_image():
    # Note: Close Putty before running this script
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=60)
        print(f"Connected to {SERIAL_PORT}. Waiting for sync header...")
    except Exception as e:
        print(f"Could not open serial port: {e}")
        return

    # 1. Wait for the sync header: [0xAA, 0xBB, 0xCC, 0xDD]
    header_pattern = b'\xAA\xBB\xCC\xDD'
    buffer = b''
    
    while True:
        char = ser.read(1)
        if not char: 
            print("Timeout waiting for header...")
            ser.close()
            return
            
        buffer = (buffer + char)[-4:] # Keep sliding window of 4 bytes
        if buffer == header_pattern:
            print("Header detected!")
            break

    # 2. Read Metadata (Width and Height) - 4 bytes total
    meta_data = ser.read(4)
    if len(meta_data) < 4:
        print("Failed to read image dimensions.")
        ser.close()
        return
        
    # Unpack two 16-bit unsigned shorts (Big Endian)
    width, height = struct.unpack('>HH', meta_data)
    print(f"Incoming Image Dimensions: {width} x {height}")

    # 3. Read (Width * Height) pixels with Progress Tracking
    expected_size = width * height
    print(f"Receiving {expected_size} bytes...")
    
    raw_data = bytearray()
    chunk_size = 1024 # Read in 1KB chunks for smoother progress updates
    
    while len(raw_data) < expected_size:
        remaining = expected_size - len(raw_data)
        # Read either the chunk size or whatever is left
        to_read = min(chunk_size, remaining)
        chunk = ser.read(to_read)
        
        if not chunk:
            print("\nError: Data stream stopped prematurely.")
            break
            
        raw_data.extend(chunk)
        print_progress_bar(len(raw_data), expected_size)
    
    ser.close()

    if len(raw_data) == expected_size:
        print("\nSuccess! Image received perfectly.")
        # Convert to numpy and reshape
        img_np = np.frombuffer(raw_data, dtype=np.uint8).reshape((height, width))
        
        # Save and Show
        img = Image.fromarray(img_np)
        img.save("received_image.png")
        print("Saved as 'received_image.png'")
        img.show()
    else:
        print(f"\nError: Received {len(raw_data)}/{expected_size} bytes.")

if __name__ == "__main__":
    capture_raw_image()