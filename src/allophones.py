"""
Compressed Allophone Data Loader for SP0256 Emulator
Generated binary file: allophones.dat
"""

import struct

class CompressedAllophoneLoader:
    def __init__(self, filename="allophones.dat"):
        self.filename = filename
        self.index = {}
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
                
                self.method = {0: 'none', 1: 'delta', 2: '4bit', 3: 'rle'}[method_byte]
                
                # Read index
                for i in range(count):
                    index_entry = f.read(8)
                    orig_len, comp_len, offset = struct.unpack('<HHI', index_entry)
                    self.index[i] = {
                        'original_length': orig_len,
                        'compressed_length': comp_len,
                        'offset': offset + 8 + (count * 8)  # Account for header and index
                    }
                    
        except Exception as e:
            print(f"Error loading allophone index: {e}")
            self.index = {}
    
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
            print(f"Error loading allophone {allophone_id}: {e}")
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
allophones = {i: None for i in _loader.get_available_allophones()}
