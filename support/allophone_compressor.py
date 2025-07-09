#!/usr/bin/env python3
"""
Allophone Data Compressor for SP0256 Emulator
Compresses allophone data for efficient storage on RP2040

Supports multiple compression methods:
1. Delta compression (differences between samples)
2. 4-bit packing (reduces from 8-bit to 4-bit samples)
3. RLE compression (run-length encoding for silence)
4. Binary format (instead of Python source)
"""

import sys
import struct
import argparse
from pathlib import Path

class AllophoneCompressor:
    def __init__(self):
        self.compression_stats = {}
        
    def delta_compress(self, samples):
        """Delta compression - store differences between samples"""
        if not samples:
            return b''
            
        # Start with first sample
        compressed = [samples[0]]
        
        # Store deltas (differences)
        for i in range(1, len(samples)):
            delta = samples[i] - samples[i-1]
            # Clamp delta to signed byte range
            delta = max(-128, min(127, delta))
            compressed.append(delta & 0xFF)
            
        return bytes(compressed)
    
    def delta_decompress(self, compressed_data):
        """Decompress delta-compressed data"""
        if not compressed_data:
            return b''
            
        samples = [compressed_data[0]]
        
        for i in range(1, len(compressed_data)):
            # Convert back to signed delta
            delta = compressed_data[i]
            if delta > 127:
                delta = delta - 256
                
            # Reconstruct sample
            new_sample = samples[-1] + delta
            new_sample = max(0, min(255, new_sample))  # Clamp to byte range
            samples.append(new_sample)
            
        return bytes(samples)
    
    def pack_4bit(self, samples):
        """Pack 8-bit samples into 4-bit (lossy compression)"""
        # Convert 8-bit (0-255) to 4-bit (0-15) centered around 0x80
        packed = []
        for i in range(0, len(samples), 2):
            # Scale from 0-255 to 0-15
            sample1 = samples[i] >> 4
            sample2 = samples[i+1] >> 4 if i+1 < len(samples) else 8
            
            # Pack two 4-bit samples into one byte
            packed_byte = (sample1 << 4) | sample2
            packed.append(packed_byte)
            
        return bytes(packed)
    
    def unpack_4bit(self, packed_data, original_length):
        """Unpack 4-bit samples back to 8-bit"""
        samples = []
        for i, packed_byte in enumerate(packed_data):
            # Extract two 4-bit samples
            sample1 = (packed_byte >> 4) & 0x0F
            sample2 = packed_byte & 0x0F
            
            # Scale back from 0-15 to 0-255
            samples.append(sample1 << 4)
            if len(samples) < original_length:
                samples.append(sample2 << 4)
                
        return bytes(samples[:original_length])
    
    def rle_compress(self, samples):
        """Simple run-length encoding"""
        if not samples:
            return b''
            
        compressed = []
        current_value = samples[0]
        count = 1
        
        for sample in samples[1:]:
            if sample == current_value and count < 255:
                count += 1
            else:
                # Store value and count
                compressed.extend([current_value, count])
                current_value = sample
                count = 1
                
        # Store final run
        compressed.extend([current_value, count])
        return bytes(compressed)
    
    def rle_decompress(self, compressed_data):
        """Decompress RLE data"""
        samples = []
        for i in range(0, len(compressed_data), 2):
            if i+1 < len(compressed_data):
                value = compressed_data[i]
                count = compressed_data[i+1]
                samples.extend([value] * count)
        return bytes(samples)
    
    def compress_allophone(self, samples, method='delta'):
        """Compress single allophone with specified method"""
        original_size = len(samples)
        
        if method == 'delta':
            compressed = self.delta_compress(samples)
        elif method == '4bit':
            compressed = self.pack_4bit(samples)
        elif method == 'rle':
            compressed = self.rle_compress(samples)
        elif method == 'none':
            compressed = samples
        else:
            raise ValueError(f"Unknown compression method: {method}")
            
        compressed_size = len(compressed)
        ratio = compressed_size / original_size if original_size > 0 else 1.0
        
        return compressed, {
            'original_size': original_size,
            'compressed_size': compressed_size,
            'ratio': ratio,
            'method': method
        }
    
    def decompress_allophone(self, compressed_data, original_length, method='delta'):
        """Decompress single allophone"""
        if method == 'delta':
            return self.delta_decompress(compressed_data)
        elif method == '4bit':
            return self.unpack_4bit(compressed_data, original_length)
        elif method == 'rle':
            return self.rle_decompress(compressed_data)
        elif method == 'none':
            return compressed_data
        else:
            raise ValueError(f"Unknown compression method: {method}")
    
    def create_binary_file(self, allophones_dict, output_file, method='delta'):
        """Create compressed binary file"""
        print(f"Creating binary file with {method} compression...")
        
        # File format:
        # Header: 4 bytes magic + 1 byte method + 1 byte count + 2 bytes reserved
        # Index: For each allophone: 2 bytes original_length + 2 bytes compressed_length + 4 bytes offset
        # Data: Compressed allophone data
        
        magic = b'SP56'  # File signature
        method_byte = {'delta': 1, '4bit': 2, 'rle': 3, 'none': 0}[method]
        count = len(allophones_dict)
        
        header = struct.pack('<4sBBH', magic, method_byte, count, 0)
        
        # Compress all allophones and build index
        index_data = b''
        compressed_data = b''
        total_original = 0
        total_compressed = 0
        
        for allophone_id in sorted(allophones_dict.keys()):
            samples = allophones_dict[allophone_id]
            compressed, stats = self.compress_allophone(samples, method)
            
            # Add to index
            original_length = len(samples)
            compressed_length = len(compressed)
            offset = len(compressed_data)
            
            index_data += struct.pack('<HHI', original_length, compressed_length, offset)
            compressed_data += compressed
            
            total_original += original_length
            total_compressed += compressed_length
            
            print(f"  Allophone {allophone_id:2d}: {original_length:4d} -> {compressed_length:4d} bytes ({stats['ratio']:.2f})")
        
        # Write file
        with open(output_file, 'wb') as f:
            f.write(header)
            f.write(index_data)
            f.write(compressed_data)
            
        total_ratio = total_compressed / total_original
        print(f"\nCompression complete:")
        print(f"  Original:   {total_original:6d} bytes")
        print(f"  Compressed: {total_compressed:6d} bytes")
        print(f"  Ratio:      {total_ratio:.3f} ({(1-total_ratio)*100:.1f}% reduction)")
        print(f"  File:       {output_file}")
        
        return total_ratio
    
    def create_micropython_loader(self, binary_file, output_py):
        """Create MicroPython loader for binary file"""
        loader_code = f'''"""
Compressed Allophone Data Loader for SP0256 Emulator
Generated binary file: {binary_file}
"""

import struct

class CompressedAllophoneLoader:
    def __init__(self, filename="{binary_file}"):
        self.filename = filename
        self.index = {{}}
        self.method = None
        self._load_index()
    
    def _load_index(self):
        """Load file header and index"""
        try:
            with open(self.filename, 'rb') as f:
                # Read header
                header = f.read(8)
                magic, method_byte, count, reserved = struct.unpack('<4sBBH', header)
                
                if magic != b'SP56':
                    raise ValueError("Invalid file format")
                
                self.method = {{0: 'none', 1: 'delta', 2: '4bit', 3: 'rle'}}[method_byte]
                
                # Read index
                for i in range(count):
                    index_entry = f.read(8)
                    orig_len, comp_len, offset = struct.unpack('<HHI', index_entry)
                    self.index[i] = {{
                        'original_length': orig_len,
                        'compressed_length': comp_len,
                        'offset': offset + 8 + (count * 8)  # Account for header and index
                    }}
                    
        except Exception as e:
            print(f"Error loading allophone index: {{e}}")
            self.index = {{}}
    
    def get_allophone(self, allophone_id):
        """Load and decompress specific allophone"""
        if allophone_id not in self.index:
            return None
            
        info = self.index[allophone_id]
        
        try:
            with open(self.filename, 'rb') as f:
                f.seek(info['offset'])
                compressed_data = f.read(info['compressed_length'])
                
            # Decompress based on method
            if self.method == 'delta':
                return self._delta_decompress(compressed_data)
            elif self.method == '4bit':
                return self._unpack_4bit(compressed_data, info['original_length'])
            elif self.method == 'rle':
                return self._rle_decompress(compressed_data)
            else:
                return compressed_data
                
        except Exception as e:
            print(f"Error loading allophone {{allophone_id}}: {{e}}")
            return None
    
    def _delta_decompress(self, compressed_data):
        """Delta decompression"""
        if not compressed_data:
            return b''
            
        samples = [compressed_data[0]]
        
        for i in range(1, len(compressed_data)):
            delta = compressed_data[i]
            if delta > 127:
                delta = delta - 256
                
            new_sample = samples[-1] + delta
            new_sample = max(0, min(255, new_sample))
            samples.append(new_sample)
            
        return bytes(samples)
    
    def _unpack_4bit(self, packed_data, original_length):
        """4-bit decompression"""
        samples = []
        for packed_byte in packed_data:
            sample1 = (packed_byte >> 4) & 0x0F
            sample2 = packed_byte & 0x0F
            
            samples.append(sample1 << 4)
            if len(samples) < original_length:
                samples.append(sample2 << 4)
                
        return bytes(samples[:original_length])
    
    def _rle_decompress(self, compressed_data):
        """RLE decompression"""
        samples = []
        for i in range(0, len(compressed_data), 2):
            if i+1 < len(compressed_data):
                value = compressed_data[i]
                count = compressed_data[i+1]
                samples.extend([value] * count)
        return bytes(samples)
    
    def get_available_allophones(self):
        """Get list of available allophone IDs"""
        return list(self.index.keys())
    
    def get_allophone_count(self):
        """Get number of available allophones"""
        return len(self.index)

# Create global loader instance
_loader = CompressedAllophoneLoader()

def get_allophone(allophone_id):
    """Get allophone data by ID"""
    return _loader.get_allophone(allophone_id)

def get_allophone_count():
    """Get number of available allophones"""
    return _loader.get_allophone_count()

# Compatibility with old interface
allophones = {{i: None for i in _loader.get_available_allophones()}}
'''
        
        with open(output_py, 'w') as f:
            f.write(loader_code)
            
        print(f"Created MicroPython loader: {output_py}")

def main():
    parser = argparse.ArgumentParser(description='Compress SP0256 allophone data')
    parser.add_argument('input_file', help='Input Python file with allophone data')
    parser.add_argument('-o', '--output', default='allophones', help='Output filename base')
    parser.add_argument('-m', '--method', choices=['delta', '4bit', 'rle', 'none'], 
                       default='delta', help='Compression method')
    parser.add_argument('--test', action='store_true', help='Test compression/decompression')
    
    args = parser.parse_args()
    
    # Import the allophone data
    sys.path.insert(0, str(Path(args.input_file).parent))
    module_name = Path(args.input_file).stem
    
    try:
        allophone_module = __import__(module_name)
        allophones_dict = allophone_module.allophones
        print(f"Loaded {len(allophones_dict)} allophones from {args.input_file}")
    except Exception as e:
        print(f"Error loading allophone data: {e}")
        return 1
    
    # Create compressor
    compressor = AllophoneCompressor()
    
    # Test compression if requested
    if args.test:
        print(f"\\nTesting {args.method} compression...")
        total_orig = 0
        total_comp = 0
        
        for aid, samples in allophones_dict.items():
            compressed, stats = compressor.compress_allophone(samples, args.method)
            decompressed = compressor.decompress_allophone(compressed, len(samples), args.method)
            
            # Verify round-trip
            if samples == decompressed:
                status = "OK"
            else:
                status = "FAILED"
                
            total_orig += stats['original_size']
            total_comp += stats['compressed_size']
            
            print(f"  Allophone {aid:2d}: {stats['ratio']:.3f} ratio - {status}")
        
        overall_ratio = total_comp / total_orig
        print(f"\\nOverall compression: {overall_ratio:.3f} ({(1-overall_ratio)*100:.1f}% reduction)")
        return 0
    
    # Create compressed binary file
    binary_file = f"{args.output}.dat"
    loader_file = f"{args.output}.py"
    
    compressor.create_binary_file(allophones_dict, binary_file, args.method)
    compressor.create_micropython_loader(binary_file, loader_file)
    
    print(f"\\nTo use in your emulator:")
    print(f"1. Copy {binary_file} and {loader_file} to your Pico")
    print(f"2. The emulator will automatically load compressed data")
    
    return 0

if __name__ == '__main__':
    sys.exit(main())