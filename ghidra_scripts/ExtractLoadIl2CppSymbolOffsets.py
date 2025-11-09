# Ghidra script to extract global variable and string constant symbols related to 'LookupSymbol' calls within the 'LoadIl2Cpp' function.
#
# This script searches for a specific pattern:
#   ...
#   LEA RDX, [s_SomeString]       ; (1) Get this string
#   ...
#   CALL LookupSymbol
#   MOV [g_SomeGlobalVar], RAX    ; (2) Get this symbol name
#   ...
#
# It then prints the extracted information in the format:
#   0xInstructionAddress, GlobalVarSymbol, StringConstantValue, 0xStringImgBaseOffset, 0xStringByteSourceOffset
#
# @category: Analysis
# @author: net2cn

from ghidra.app.script import GhidraScript
from ghidra.program.model.listing import Instruction
from ghidra.program.model.data import DataUtilities
from ghidra.program.model.symbol import SymbolTable
from ghidra.program.model.address import Address
from ghidra.program.model.lang import OperandType

# Get the program's listing and symbol table
listing = currentProgram.getListing()
symbol_table = currentProgram.getSymbolTable()
# Get the Memory object to find file offsets
memory = currentProgram.getMemory()

def get_byte_source_offset(addr):
    # --- Get the actual file offset (byte source offset) ---
    byte_source_offset = -1
    block = memory.getBlock(addr)
    
    if block and block.isLoaded():
        source_infos = block.getSourceInfos()
        if source_infos:
            for info in source_infos:
                # Check if this source info block contains our address
                if info.contains(addr):
                    try:
                        byte_source_offset = info.getFileBytesOffset(addr)
                        break
                    except Exception as e:
                        printerr("  [!] Error getting byte source offset: {}".format(e))
    return byte_source_offset

def getFunctionCalled(call_instruction):
    """
    Helper function to get the Function object called by a CALL instruction.
    """
    if call_instruction.getMnemonicString() != "CALL":
        return None
        
    refs = call_instruction.getReferencesFrom()
    if refs:
        to_addr = refs[0].getToAddress()
        if to_addr:
            return getFunctionAt(to_addr)
    return None

def run():
    TARGET_FUNCTION = "LoadIl2Cpp"
    CALL_TARGET = "LookupSymbol"
    MAX_LOOKBACK = 5  # How many instructions to look back for LEA at max
    MAX_LOOKAHEAD = 5 # How many instructions to look forward for MOV at max

    # Find the target function
    funcs = getGlobalFunctions(TARGET_FUNCTION)
    if not funcs:
        printerr("Function '{}' not found.".format(TARGET_FUNCTION))
        return
    
    func = funcs[0]
    print("--- Analyzing function: {} ---".format(func.getName()))

    # Iterate through all instructions in the function
    instructions = listing.getInstructions(func.getBody(), True)
    
    for inst in instructions:
        
        if inst.getMnemonicString() != "CALL":
            continue

        # Check if this is a CALL to our target
        called_func = getFunctionCalled(inst)
        if called_func and called_func.getName() == CALL_TARGET:
            
            call_inst = inst
            
            # --- 1. Find the LEA RDX, [string_addr] (looking backward) ---
            string_addr = None
            string_value = None
            
            curr = call_inst
            for _ in range(MAX_LOOKBACK):
                curr = curr.getPrevious()
                if curr is None: break

                
                # Check for LEA RDX, [address]
                if (curr.getMnemonicString() == "LEA" and
                    curr.getNumOperands() > 1 and
                    curr.getOpObjects(0) and
                    curr.getOpObjects(0)[0].toString() == "RDX"):
                    
                    # Get address from the second operand's references
                    # This is more reliable than getOpObjects()
                    refs = curr.getReferencesFrom()
                    if refs and len(refs) > 0:
                        string_addr = refs[0].getToAddress()
            
            if not string_addr:
                printerr("  [!] Could not find LEA for string at {}".format(call_inst.getAddress()))
                continue
                
            # Get the string data at the found address
            data = getDataAt(string_addr)
            if data:
                string_value = data.getValue()
            else:
                printerr("  [!] No defined data (string) at {}".format(string_addr))
                continue

            # --- 2. Find the MOV [symbol_addr], RAX (looking forward) ---
            symbol_name = None
            
            curr = call_inst
            for _ in range(MAX_LOOKAHEAD):
                curr = curr.getNext()
                if curr is None: break
                
                # Check for MOV [address], RAX
                if (curr.getMnemonicString() == "MOV" and
                    curr.getNumOperands() > 1 and
                    (curr.getOperandType(0) & OperandType.ADDRESS) and # Dest is address/memory
                    curr.getOpObjects(1) and
                    curr.getOpObjects(1)[0].toString() == "RAX"): # Source is RAX
                    
                    # Get the destination address (operand 0)
                    dest_addr_obj = curr.getOpObjects(0)[0]
                    if isinstance(dest_addr_obj, Address):
                        # Get the primary symbol at that address
                        symbol = symbol_table.getPrimarySymbol(dest_addr_obj)
                        if symbol:
                            symbol_name = symbol.getName()
                            break
            
            if not symbol_name:
                printerr("  [!] Could not find MOV symbol after {}".format(call_inst.getAddress()))
                continue

            # --- 3. Print the result ---
            # Format: call_address, symbol_name, "string_value", string_address
            output = '0x{}, {}, "{}", 0x{:x}, 0x{:x}'.format(call_inst.getAddress(), symbol_name, string_value, string_addr.getOffset(), get_byte_source_offset(string_addr))
            print(output)
            
    print("--- Analysis complete ---")

# Instantiate and run the script
run()