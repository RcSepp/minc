import os
import re

################################################################################
# Classes
################################################################################

class UnknownTypeException(Exception):
    def __init__(self, typestr):
        super().__init__("Unknown type: " + typestr)
        self.typestr = typestr

class FuncDecl(object):
    def __init__(self, attrs, restype, name, argtypes):
        self.attrs = attrs
        self.restype = restype
        self.c_restype = None
        self.name = name
        self.argtypes = argtypes
        self.c_args = None
        self.c_argtypes = None
        self.modified_name = None

################################################################################
# Regexes
################################################################################

IR_TYPE_REGEX = re.compile(r"(?:%\"[^\"]*\")|%[\w.:]+")
IR_TYPEDEF_REGEX = re.compile(r"(.*?) = type (.*)")
IR_FUNCDEF_REGEX = re.compile(r"define (dso_local)? (.*?) @([^(]+)\((.*)\) (?:local_unnamed_addr )?#.*")
C_TYPE_REGEX = re.compile(r"(.*?)[^\*\s]+$")
C_FUNCDEF_REGEX = re.compile(r"([\w\*\s]+[\*\s])(\w+)\s*\(([^\)]*)\)\s*{")

template_regex = re.compile(r"@(\t*)(\w+)@")

################################################################################
# Globals
################################################################################

FILE_HEADER = """// This file was generated by llvm-c-decl-extractor.py.
// Do not edit this file directly!
// To edit this file, edit the template in "/llvm_constants/templates" and rerun llvm-c-decl-extractor.py.
"""

types = {}
decls = {}

################################################################################
# Helper functions
################################################################################

def split_outside_brackets(sentence, seperator):
    wordstart = 0
    words = []
    bracket_level = 0
    for i, c in enumerate(sentence):
        if c == "(":
            bracket_level += 1
        elif c == ")":
            bracket_level -= 1
        elif bracket_level == 0 and i + len(seperator) <= len(sentence) and all(sentence[i + j] == sc for j, sc in enumerate(seperator)):
            words.append(sentence[wordstart:i])
            wordstart = i + len(seperator)
    if wordstart + 1 < len(sentence):
        words.append(sentence[wordstart:])
    return words

def get_name(llvmid):
    name = llvmid.lstrip('%').replace('.', '_').replace('::', '_')
    if name.startswith("struct"): name = name[len("struct"):]
    return name.strip('_')

def get_builtin_type(typestr):
    typestr = typestr \
        .replace("noalias", "") \
        .replace("nocapture", "") \
        .replace("nonnull", "") \
        .replace("readonly", "") \
        .replace("readnone", "") \
        .replace("zeroext", "") \
        .strip()

    rawtypestr = typestr.rstrip('*')
    ptrlevel = len(typestr) - len(rawtypestr)

    # if rawtypestr.endswith("Ref"):
    #     rawtypestr = rawtypestr[:-3]
    #     ptrlevel += 1

    if rawtypestr == 'void':
        if ptrlevel:
            typestr = "BuiltinTypes::VoidPtr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Void"
    elif rawtypestr == 'i8':
        if ptrlevel:
            typestr = "BuiltinTypes::Int8Ptr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Int8"
    elif rawtypestr == 'i16':
        if ptrlevel:
            typestr = "BuiltinTypes::Int16Ptr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Int16"
    elif rawtypestr == 'i32':
        if ptrlevel:
            typestr = "BuiltinTypes::Int32Ptr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Int32"
    elif rawtypestr == 'i64':
        if ptrlevel:
            typestr = "BuiltinTypes::Int64Ptr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Int64"
    elif rawtypestr == 'half':
        if ptrlevel:
            typestr = "BuiltinTypes::HalfPtr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Half"
    elif rawtypestr == 'float':
        if ptrlevel:
            typestr = "BuiltinTypes::FloatPtr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Float"
    elif rawtypestr == 'double':
        if ptrlevel:
            typestr = "BuiltinTypes::DoublePtr"
            ptrlevel -= 1
        else:
            typestr = "BuiltinTypes::Double"
    elif rawtypestr in types:
        typestr = get_name(rawtypestr)
        if typestr.startswith("LLVMOpaque"):
            typestr = "LLVM" + typestr[len("LLVMOpaque"):]
            if not typestr.endswith("Ref"):
                typestr += "Ref"
                ptrlevel -= 1
        typestr = "BuiltinTypes::" + typestr
    else:
        raise UnknownTypeException(typestr)

    for _ in range(ptrlevel):
        typestr += "->Ptr()"

    return typestr

def get_c_type(c_typestr):
    c_typestr = c_typestr.strip()

    match = C_TYPE_REGEX.fullmatch(c_typestr)
    if match and match.group(1):
        return match.group(1).strip()
    else:
        return c_typestr

################################################################################
# Parse IR files
################################################################################

missing_decls = set()
for filename in ['Core.ll', 'CloneModule.ll', 'DebugInfo.ll']:
    with open(filename, 'r') as file:
        for line in file:
            line = line.strip('\n')
            
            match = IR_TYPEDEF_REGEX.fullmatch(line)
            if match:
                #print(match.group(1), match.group(2))
                types[match.group(1)] = [False, match.group(2)]

            match = IR_FUNCDEF_REGEX.fullmatch(line)
            if match and not match.group(3).startswith('_'): # Ignore C++ functions starting with '_'
                decl = FuncDecl(*match.groups())
                decl.name = get_name(decl.name)
                decl.argtypes = split_outside_brackets(decl.argtypes, ',')
                decl.argtypes = [ argtype.strip() for argtype in decl.argtypes ]
                decls[decl.name] = decl

################################################################################
# Parse header files
################################################################################

for filename in ['Core.cpp', 'DebugInfo.cpp', 'CloneModule.cpp']:
    with open(filename, 'r') as file:
        for match in C_FUNCDEF_REGEX.finditer(file.read()):
            decl_name = match.group(2)
            try:
                decl = decls[decl_name]
            except KeyError:
                missing_decls.add(decl_name)
                continue
            decl.c_restype = get_c_type(match.group(1))
            decl.c_args = match.group(3).replace('\n', ' ').replace('\t', ' ')
            decl.c_args = split_outside_brackets(decl.c_args, ',')
            decl.c_args = [ c_arg.strip() for c_arg in decl.c_args ]
            decl.c_argtypes = [ get_c_type(c_arg) for c_arg in decl.c_args ]

# Report function declarations found in header file but missing from IR file
if missing_decls:
    print(str(len(missing_decls)) + " MISSING DECLS FOUND:")
    for d in missing_decls:
        print(d)
    #raise Exception("Unknown decls found")

# Report function declarations found in IR file but missing from header file
missing_decls = set()
for decl in decls.values():
    if decl.c_restype is None:
        missing_decls.add(decl.name)
if missing_decls:
    print(str(len(missing_decls)) + " MISSING DECLS FOUND:")
    for d in missing_decls:
        print(d)
    #raise Exception("Unknown decls found")
    decls = { k: v for k, v in decls.items() if k not in missing_decls }

num_used = sum(1 for t in types.values() if t[0])
num_unused = len(types)
print(num_used, num_unused)

# Mark all types in decls as used
for decl in decls.values():
    for tpe in [decl.restype] + decl.argtypes:
        match = IR_TYPE_REGEX.search(tpe)
        if match:
            match = match.group(0)
            types[match][0] = True

num_used = sum(1 for t in types.values() if t[0])
num_unused = len(types)
print(num_used, num_unused)

# Repeatedly mark types used by used types as used
for i in range(1):
    for t, tb in types.items():
        if tb[0] == True:
            for match in IR_TYPE_REGEX.finditer(tb[1]):
                match = match.group(0)
                types[match][0] = True

    num_used = sum(1 for t in types.values() if t[0])
    num_unused = len(types)
    print(num_used, num_unused)

types = { t: tb[1] for t, tb in types.items() if tb[0] }

print(len(decls))

################################################################################
# Filter functions
################################################################################

SKIP_FUNCS = [
    'LLVMPositionBuilder',
    'LLVMBuildInBoundsGEP1',
    'LLVMBuildInBoundsGEP2',
]
for skip_func in SKIP_FUNCS:
    try:
        del decls[skip_func]
    except KeyError:
        pass

################################################################################
# Modify functions
################################################################################

REPLACE_PARAMS = {
    'LLVMBuilderRef Builder': 'wrap(builder)',
    'LLVMBuilderRef B': 'wrap(builder)',
    'LLVMModuleRef Mod': 'wrap(currentModule)',
    'LLVMModuleRef M': 'wrap(currentModule)',
    'LLVMDIBuilderRef Builder': 'wrap(dbuilder)',
}
for decl in decls.values():
    for c_arg in decl.c_args:
        if c_arg in REPLACE_PARAMS:
            decl.modified_name = "LLVMEX" + decl.name[4:]
            break

for filename in os.listdir("./templates"):
    template_path = os.path.join("./templates", filename)
    if os.path.isdir(template_path): continue

    ############################################################################
    # Read template
    ############################################################################

    template = open(template_path, 'r').read()
    template_regex.fullmatch(template)

    def repl(match):
        template_indent = match.group(1)
        template_name = match.group(2)
        if template_name == "LLVM_EXTERN_FUNC_DEF":

            ####################################################################
            # Print extern function declarations
            ####################################################################

            output = []
            unknown_types = set()
            for decl in decls.values():
                try:
                    restype = get_builtin_type(decl.restype)
                    argtypes = [get_builtin_type(typestr) for typestr, c_arg in zip(decl.argtypes, decl.c_args) if c_arg not in REPLACE_PARAMS]
                except UnknownTypeException as err:
                    unknown_types.add(err.typestr)
                    continue

                output.append(
                    'llvm_c_functions.push_back(Func("{}", {}, {{ {} }}, false{}));'
                    .format(
                        decl.name,
                        restype,
                        ", ".join(argtypes),
                        ', "{}"'.format(decl.modified_name) if decl.modified_name else ""
                    )
                )

            # Report unknown file types
            if unknown_types:
                print(str(len(unknown_types)) + " UNKNOWN TYPES FOUND:")
                for t in unknown_types:
                    print(t)
                #raise Exception("Unknown types found")

            return template_indent + ("\n" + template_indent).join(output)

        elif template_name == "LLVM_TYPE_DECL":

            ####################################################################
            # Print type declarations
            ####################################################################

            output = []
            for t, tb in types.items():
                t_id = t[1:]
                t_name = get_name(t)
                output.append('StructType* {};'.format(t_name, t_id))
            return template_indent + ("\n" + template_indent).join(output)

        elif template_name == "LLVM_TYPE_EXTERN_DECL":

            ####################################################################
            # Print extern type declarations
            ####################################################################

            output = []
            for t, tb in types.items():
                t_id = t[1:]
                t_name = get_name(t)
                output.append('extern StructType* {};'.format(t_name, t_id))
            return template_indent + ("\n" + template_indent).join(output)

        elif template_name == "LLVM_TYPE_DEF":

            ####################################################################
            # Print type definitions
            ####################################################################

            output = []
            for t, tb in types.items():
                t_id = t[1:]
                t_name = get_name(t)
                output.append('{} = StructType::create(c, "{}");'.format(t_name, t_id))
            return template_indent + ("\n" + template_indent).join(output)

        elif template_name == "MODIFIED_LLVM_EXTERN_FUNC_DEF":

            ####################################################################
            # Print modified extern functions
            ####################################################################

            output = []
            for decl in decls.values():
                if decl.modified_name:
                    restype = decl.c_restype

                    args = []
                    argvals = []
                    for c_arg, c_argtype in zip(decl.c_args, decl.c_argtypes):
                        argval = REPLACE_PARAMS.get(c_arg)
                        if argval is None:
                            argval = c_arg[len(c_argtype):].strip()
                            args.append(c_arg)
                        argvals.append(argval)

                    output.append(
                        '{} {}({}) {{ return {}({}); }}'
                        .format(
                            restype,
                            decl.modified_name,
                            ", ".join(args),
                            decl.name,
                            ", ".join(argvals)
                        )
                    )

            return template_indent + ("\n" + template_indent).join(output)

    template = template_regex.sub(repl, template)

    ############################################################################
    # Write output
    ############################################################################

    output_path = os.path.join("../src", filename)
    with open(output_path, 'w') as out:
        out.write(FILE_HEADER)
        out.write(template)
