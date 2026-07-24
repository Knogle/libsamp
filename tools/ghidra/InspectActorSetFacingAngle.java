// STATIC_037 focused metadata export for the SA-MP 0.3.7-R5 RPC 175 path.
// Emits bounded instruction/reference/operand metadata only; no decompiler
// pseudocode.

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.nio.file.Files;

public class InspectActorSetFacingAngle extends GhidraScript {
    private static final long IMAGE_BASE = 0x10000000L;

    private static final FocusFunction[] FOCUS_FUNCTIONS = new FocusFunction[] {
        new FocusFunction("rpc175_handler", 0x1001d9f0L),
        new FocusFunction("cactor_set_facing_angle", 0x1009c570L),
        new FocusFunction("angle_conversion_helper", 0x100b5970L)
    };

    private File outDir;

    private static class FocusFunction {
        final String label;
        final long address;

        FocusFunction(String label, long address) {
            this.label = label;
            this.address = address;
        }
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) {
            throw new IllegalArgumentException(
                "usage: InspectActorSetFacingAngle.java <output-dir>");
        }

        outDir = new File(args[0]);
        Files.createDirectories(outDir.toPath());

        exportIdentity();
        exportFocusMetadata();
        exportFocusMemory();
    }

    private void exportIdentity() throws Exception {
        BufferedWriter identity = writer("identity.tsv");
        identity.write("property\tvalue\n");
        identity.write(tsv("program_name", currentProgram.getName()));
        identity.write(tsv("executable_path", currentProgram.getExecutablePath()));
        identity.write(tsv("executable_format", currentProgram.getExecutableFormat()));
        identity.write(tsv("executable_md5", currentProgram.getExecutableMD5()));
        identity.write(tsv("executable_sha256", currentProgram.getExecutableSHA256()));
        identity.write(tsv("image_base", currentProgram.getImageBase()));
        identity.write(tsv("language_id", currentProgram.getLanguageID()));
        identity.write(tsv("compiler_spec_id",
            currentProgram.getCompilerSpec().getCompilerSpecID()));
        identity.close();
    }

    private void exportFocusMetadata() throws Exception {
        BufferedWriter functions = writer("functions.tsv");
        BufferedWriter instructions = writer("instructions.tsv");
        BufferedWriter calls = writer("calls.tsv");
        BufferedWriter branches = writer("branches.tsv");
        BufferedWriter data = writer("data_refs.tsv");
        BufferedWriter scalars = writer("scalars.tsv");
        BufferedWriter incoming = writer("incoming_refs.tsv");

        functions.write("label\tfunction\tfunction_rva\tsize\tinstruction_count\tcall_in\tcall_out"
            + "\tdata_refs_from\tbranch_count\tcalling_convention\tentry_bytes_16\n");
        instructions.write("label\tfunction_rva\tinsn_rva\tbytes\tmnemonic\toperand0\toperand1"
            + "\toperand2\tflow_type\tfallthrough_rva\n");
        calls.write("label\tfunction_rva\tcallsite_rva\tbytes\tmnemonic\toperand\ttarget"
            + "\ttarget_rva\tref_type\n");
        branches.write("label\tfunction_rva\tinsn_rva\tbytes\tmnemonic\toperand\tflow_type"
            + "\ttarget_rva\tfallthrough_rva\n");
        data.write("label\tfunction_rva\tinsn_rva\tbytes\tmnemonic\toperand\ttarget"
            + "\ttarget_rva\tref_type\n");
        scalars.write("label\tfunction_rva\tinsn_rva\tmnemonic\toperand_index\tvalue\toperand\n");
        incoming.write("label\tfunction_rva\tfrom_function\tfrom_function_rva\tfrom_rva\tref_type\n");

        for (FocusFunction focus : FOCUS_FUNCTIONS) {
            Function function = getFunctionContaining(toAddr(focus.address));
            if (function == null ||
                function.getEntryPoint().getOffset() != focus.address) {
                functions.write(tsv(focus.label, "missing", rva(focus.address),
                    0, 0, 0, 0, 0, 0, "", ""));
                continue;
            }

            int callIn = 0;
            int callOut = 0;
            int dataRefs = 0;
            int branchCount = 0;
            int instructionCount = 0;

            ReferenceIterator incomingRefs =
                currentProgram.getReferenceManager().getReferencesTo(function.getEntryPoint());
            while (incomingRefs.hasNext() && !monitor.isCancelled()) {
                Reference ref = incomingRefs.next();
                Function from = getFunctionContaining(ref.getFromAddress());
                incoming.write(tsv(focus.label,
                    rva(function.getEntryPoint().getOffset()),
                    from == null ? "" : from.getName(),
                    from == null ? "" : rva(from.getEntryPoint().getOffset()),
                    rva(ref.getFromAddress().getOffset()), ref.getReferenceType()));
                if (ref.getReferenceType().isCall()) {
                    callIn++;
                }
            }

            InstructionIterator iterator =
                currentProgram.getListing().getInstructions(function.getBody(), true);
            while (iterator.hasNext() && !monitor.isCancelled()) {
                Instruction instruction = iterator.next();
                instructionCount++;

                String bytes = bytes(instruction);
                String fallthrough = instruction.getFallThrough() == null
                    ? "" : rva(instruction.getFallThrough().getOffset());
                instructions.write(tsv(focus.label,
                    rva(function.getEntryPoint().getOffset()),
                    rva(instruction.getAddress().getOffset()), bytes,
                    instruction.getMnemonicString(), operand(instruction, 0),
                    operand(instruction, 1), operand(instruction, 2),
                    instruction.getFlowType(), fallthrough));

                boolean emittedCallReference = false;
                for (Reference ref : instruction.getReferencesFrom()) {
                    Function target = getFunctionContaining(ref.getToAddress());
                    if (ref.getReferenceType().isCall()) {
                        calls.write(tsv(focus.label,
                            rva(function.getEntryPoint().getOffset()),
                            rva(instruction.getAddress().getOffset()), bytes,
                            instruction.getMnemonicString(), instruction.toString(),
                            target == null ? ref.getToAddress() : target.getName(),
                            rva(ref.getToAddress().getOffset()),
                            ref.getReferenceType()));
                        callOut++;
                        emittedCallReference = true;
                    } else if (ref.getReferenceType().isData()) {
                        data.write(tsv(focus.label,
                            rva(function.getEntryPoint().getOffset()),
                            rva(instruction.getAddress().getOffset()), bytes,
                            instruction.getMnemonicString(), instruction.toString(),
                            ref.getToAddress(), rva(ref.getToAddress().getOffset()),
                            ref.getReferenceType()));
                        dataRefs++;
                    }
                }

                if (instruction.getFlowType().isCall() &&
                    !emittedCallReference) {
                    calls.write(tsv(focus.label,
                        rva(function.getEntryPoint().getOffset()),
                        rva(instruction.getAddress().getOffset()), bytes,
                        instruction.getMnemonicString(), instruction.toString(),
                        "indirect", "", "INDIRECT"));
                    callOut++;
                }

                if (instruction.getFlowType().isJump() ||
                    instruction.getFlowType().isConditional() ||
                    instruction.getFlowType().isTerminal()) {
                    Address[] flows = instruction.getFlows();
                    if (flows.length == 0) {
                        branches.write(tsv(focus.label,
                            rva(function.getEntryPoint().getOffset()),
                            rva(instruction.getAddress().getOffset()), bytes,
                            instruction.getMnemonicString(), instruction.toString(),
                            instruction.getFlowType(), "", fallthrough));
                    } else {
                        for (Address target : flows) {
                            branches.write(tsv(focus.label,
                                rva(function.getEntryPoint().getOffset()),
                                rva(instruction.getAddress().getOffset()), bytes,
                                instruction.getMnemonicString(), instruction.toString(),
                                instruction.getFlowType(), rva(target.getOffset()),
                                fallthrough));
                        }
                    }
                    branchCount++;
                }

                for (int operandIndex = 0;
                     operandIndex < instruction.getNumOperands();
                     operandIndex++) {
                    for (Object object :
                        instruction.getOpObjects(operandIndex)) {
                        if (object instanceof Scalar) {
                            long value = ((Scalar)object).getUnsignedValue();
                            scalars.write(tsv(focus.label,
                                rva(function.getEntryPoint().getOffset()),
                                rva(instruction.getAddress().getOffset()),
                                instruction.getMnemonicString(), operandIndex,
                                String.format("0x%x", value),
                                instruction.getDefaultOperandRepresentation(
                                    operandIndex)));
                        }
                    }
                }
            }

            functions.write(tsv(focus.label, function.getName(),
                rva(function.getEntryPoint().getOffset()),
                function.getBody().getNumAddresses(), instructionCount,
                callIn, callOut, dataRefs, branchCount,
                function.getCallingConventionName(),
                bytes(function.getEntryPoint(), 16)));
        }

        functions.close();
        instructions.close();
        calls.close();
        branches.close();
        data.close();
        scalars.close();
        incoming.close();
    }

    private void exportFocusMemory() throws Exception {
        BufferedWriter memory = writer("focus_memory.tsv");
        memory.write("label\trva\tbytes\n");
        memory.write(tsv("rpc175_entry_window", rva(0x1001d9f0L),
            bytes(toAddr(0x1001d9f0L), 32)));
        memory.write(tsv("rpc175_registration_window",
            rva(0x1001e70dL), bytes(toAddr(0x1001e70dL), 22)));
        memory.write(tsv("rpc175_id_value",
            rva(0x100e6260L), bytes(toAddr(0x100e6260L), 4)));
        memory.write(tsv("set_facing_angle_entry_window",
            rva(0x1009c570L), bytes(toAddr(0x1009c570L), 32)));
        memory.write(tsv("ped_ref_lookup_wrapper_entry",
            rva(0x100b3b70L), bytes(toAddr(0x100b3b70L), 32)));
        memory.write(tsv("angle_conversion_entry_window",
            rva(0x100b5970L), bytes(toAddr(0x100b5970L), 32)));
        memory.write(tsv("conversion_constant_e6124",
            rva(0x100e6124L), bytes(toAddr(0x100e6124L), 4)));
        memory.write(tsv("conversion_constant_e5940",
            rva(0x100e5940L), bytes(toAddr(0x100e5940L), 4)));
        memory.write(tsv("conversion_constant_ed468",
            rva(0x100ed468L), bytes(toAddr(0x100ed468L), 4)));
        memory.write(tsv("conversion_constant_ed47c",
            rva(0x100ed47cL), bytes(toAddr(0x100ed47cL), 4)));
        memory.write(tsv("conversion_constant_ed480",
            rva(0x100ed480L), bytes(toAddr(0x100ed480L), 4)));
        memory.write(tsv("netgame_global_initial_value",
            rva(0x1026eb94L), bytes(toAddr(0x1026eb94L), 4)));
        memory.close();
    }

    private String operand(Instruction instruction, int index) {
        if (index >= instruction.getNumOperands()) {
            return "";
        }
        return instruction.getDefaultOperandRepresentation(index);
    }

    private String bytes(Instruction instruction) {
        return bytes(instruction.getAddress(), instruction.getLength());
    }

    private String bytes(Address address, int count) {
        byte[] values = new byte[count];
        try {
            int read = currentProgram.getMemory().getBytes(address, values);
            StringBuilder result = new StringBuilder();
            for (int index = 0; index < read; index++) {
                if (index > 0) {
                    result.append(' ');
                }
                result.append(String.format("%02x", values[index] & 0xff));
            }
            return result.toString();
        } catch (Exception exception) {
            return "unreadable:" + exception.getClass().getSimpleName();
        }
    }

    private BufferedWriter writer(String name) throws Exception {
        return new BufferedWriter(new FileWriter(new File(outDir, name)));
    }

    private String tsv(Object... columns) {
        StringBuilder result = new StringBuilder();
        for (int index = 0; index < columns.length; index++) {
            if (index > 0) {
                result.append('\t');
            }
            result.append(String.valueOf(columns[index])
                .replace("\t", " ").replace("\n", "\\n"));
        }
        result.append('\n');
        return result.toString();
    }

    private String rva(long address) {
        return String.format("0x%08x", address - IMAGE_BASE);
    }
}
