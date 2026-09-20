#!/usr/bin/env python3
"""Build a Unix V6 filesystem image from a mkfs `proto` file.

A readable reimplementation of V6 `mkfs` (source/s2/mkfs.c, as ported by
RealXV6's tools/mkfs.c).  Same filesystem, none of the PDP-11 scaffolding.

Image layout (little-endian, 512-byte blocks):

    0                   boot sector, copied from the proto's first file
    1                   superblock
    2 .. isize+1        inodes, 16 per block, 32 bytes each, inode 1 = root
    isize+2 .. fsize-1  data blocks, handed out from the free list
    fsize .. fsize+swap swap area: reserved, never in the free list

Differences from the C: i_size0 is written, so files of 64 KB or more record
their true 24-bit size (the C carried nothing past i_size1); each entry's
content is written in one go, so block numbers differ; the rk/rp free-list
interleave is gone, having keyed off the image *filename*.  Timestamps stay
0 and text is written LF (is_text_source), so the image is reproducible:
the same tree gives the same image, on any host and from any checkout.

Usage:
    python mkfs.py IMAGE PROTO [-D NAME=PATH]...
"""
import argparse
import re
import struct
import sys
from dataclasses import dataclass, field
from enum import IntEnum
from pathlib import Path

BLOCK = 512
INODE_SIZE = 32
INODES_PER_BLOCK = BLOCK // INODE_SIZE          # 16
FIRST_INODE_BLOCK = 2
NAME_MAX = 14                                   # a directory entry's name field
DIRECT_ADDRS = 8                                # i_addr[] slots in an inode
ADDRS_PER_BLOCK = BLOCK // 2                    # block numbers in an indirect
FREE_PER_BLOCK = 100                            # s_free[] slots

IALLOC = 0o100000                               # inode is in use
ILARG = 0o010000                                # i_addr[0] is an indirect block
ISUID = 0o004000
ISGID = 0o002000
IFMT = 0o060000                                 # file-type mask


class FileType(IntEnum):
    """The IFMT field of i_mode."""
    REGULAR = 0o000000
    CHAR = 0o020000
    DIRECTORY = 0o040000
    BLOCK = 0o060000


TYPE_CHARS = {"-": FileType.REGULAR, "b": FileType.BLOCK,
              "c": FileType.CHAR, "d": FileType.DIRECTORY}
MODE_RE = re.compile(r"([-bcd])([u-])([g-])([0-7]{3})")

# Source suffixes that go onto the image as text, hence as LF.  ".A" is
# assembler source; ".S" is DeSmet's library format and is NOT here.
TEXT_SUFFIXES = {".c", ".h", ".a", ".asm", ".ed", ".rsp"}


def is_text_source(path):
    """Is this host file something the target reads as text?

    Every text file reaches the image with LF line endings, whatever the
    host has on disk.  The target is a Unix: it has no DOS text mode, so a
    CR is simply a byte in the line, and whether one is there must not
    depend on a checkout convention -- a published tree carries no
    .gitattributes, and `core.autocrlf` differs from machine to machine.
    Most readers on the image cope with a CR anyway (c88 classes it as
    white space, asm88 and BIND's -f reader do the same, drun strips it),
    but ed does not: it takes a CR where it wants a newline as a syntax
    error, seeks its command input to EOF and exits having written
    nothing, so `ed KEN/MAIN.C <banner.ed` silently edits nothing.
    Normalizing here fixes that for every reader at once, and makes the
    image a function of this tree's content rather than of the checkout it
    was built from -- which is what lets the byte counts in the evidence
    logs be reproduced.

    Decided by name, never by sniffing the bytes: the image also carries
    a.out binaries (.AO/.aout), objects (.O), a DeSmet library (.S), .COM
    kernels, .EXE links and a floppy golden, and a CR LF pair inside one of
    those is data.  The extensionless entries are all scripts -- the drun
    scripts under usr/dsrc and usr/dtest.  A text file whose suffix is
    missing from the list keeps its CRs and still works; that is the safe
    direction for this list to be wrong in, and content() rejects the
    unsafe one (a NUL byte in something named like text).
    """
    return path.suffix == "" or path.suffix.lower() in TEXT_SUFFIXES


class MkfsError(Exception):
    """A malformed proto file, a missing input, or a filesystem too small."""


# --- the tree described by the proto file ---------------------------------

@dataclass
class Node:
    """One filesystem entry.  `parent` is the node itself for the root."""
    name: str
    mode: int                                   # complete i_mode
    uid: int
    gid: int
    origin: str                                 # 'proto line 12', for errors
    source: Path = None                         # regular file: host file to copy
    dev: tuple = None                           # special file: (major, minor)
    children: list = field(default_factory=list)
    parent: "Node" = field(default=None, repr=False)
    ino: int = 0                                # assigned by build(), in preorder

    @property
    def type(self):
        return FileType(self.mode & IFMT)

    @property
    def is_dir(self):
        return self.type is FileType.DIRECTORY

    @property
    def nlink(self):
        """A directory's own '.', its parent's entry, and each subdirectory's
        '..'.  For '/' the parent entry is its own '..', so the count holds."""
        if not self.is_dir:
            return 1
        return 2 + sum(1 for child in self.children if child.is_dir)

    def content(self):
        """This entry's bytes.  A directory's content is its entry table, so
        every inode number must be assigned before anything is written.

        Text arrives as LF: see is_text_source()."""
        if self.is_dir:
            entries = [(self.parent.ino, ".."), (self.ino, ".")]
            entries += [(child.ino, child.name) for child in self.children]
            return b"".join(dirent(ino, name) for ino, name in entries)
        try:
            data = self.source.read_bytes()
        except OSError as exc:
            raise MkfsError("%s: cannot read %s: %s"
                            % (self.origin, self.source, exc.strerror))
        if is_text_source(self.source):
            if b"\0" in data:
                raise MkfsError(
                    "%s: %s is named like text but holds NUL bytes; mkfs "
                    "would have stripped CRs out of a binary" % (self.origin,
                                                                 self.source))
            data = data.replace(b"\r\n", b"\n")
        return data


def dirent(ino, name):
    """A 16-byte directory entry: inode number, NUL-padded name."""
    return struct.pack("<H%ds" % NAME_MAX, ino, name.encode("ascii"))


def preorder(node):
    """Parents before children: the order inodes are numbered and written."""
    yield node
    for child in node.children:
        yield from preorder(child)


# --- the proto file -------------------------------------------------------

class ProtoParser:
    """Reads a proto file into a tree of Nodes.

    Words are whitespace-separated; newlines and indentation are decoration.
    A directory ends at a word of '$', and a word starting with ':' comments
    out the rest of its line.
    """

    def __init__(self, path, defines):
        self.path = Path(path)
        self.defines = defines
        self.words = list(self._scan(self.path.read_text()))
        self.pos = 0
        self.line = 0

    @staticmethod
    def _scan(text):
        for lineno, line in enumerate(text.splitlines(), 1):
            for word in line.split():
                if word.startswith(":"):
                    break
                yield word, lineno

    def parse(self):
        """Returns (boot sector path or None, (fsize, isize, swap), root node).

        A lone '-' on the first line means no boot sector: block 0 stays zero.
        Only the root disk is booted; rk1 and up carry data, and writing a
        bootable-looking sector on them would be a lie."""
        boot = None if self.peek_dash() else self.source_path(
            "the boot sector file or '-'")
        geometry = tuple(self.number(n) for n in ("fsize", "isize", "swap"))
        root = self.entry("/")
        if not root.is_dir:
            raise self.error("the root entry must be a directory, as in "
                             "'d--755 0 0'")
        if self.pos < len(self.words):
            raise self.error("trailing word %r after the root directory was "
                             "closed" % self.words[self.pos][0])
        return boot, geometry, root

    # -- the entry grammar --------------------------------------------------

    def entry(self, name, parent=None):
        """A mode, uid and gid, then whatever the type needs: a host path, a
        device number pair, or a nested directory listing."""
        node = Node(name, self.mode(), self.number("a uid"),
                    self.number("a gid"), self.origin())
        node.parent = parent or node
        if node.is_dir:
            self.directory(node)
        elif node.type is FileType.REGULAR:
            node.source = self.source_path("a source path")
        else:
            node.dev = (self.number("a major device number"),
                        self.number("a minor device number"))
        return node

    def directory(self, node):
        """Entries until the '$' that closes this directory."""
        while True:
            name = self.word("a directory entry or '$'")
            if name == "$":
                return
            if len(name) > NAME_MAX:
                raise self.error("name %r is longer than %d characters"
                                 % (name, NAME_MAX))
            node.children.append(self.entry(name, node))

    # -- words --------------------------------------------------------------

    def word(self, expected):
        if self.pos >= len(self.words):
            raise MkfsError("%s: file ends while expecting %s"
                            % (self.path.name, expected))
        text, self.line = self.words[self.pos]
        self.pos += 1
        return text

    def number(self, expected):
        text = self.word(expected)
        if not text.isdigit():
            raise self.error("%s must be a number, got %r" % (expected, text))
        return int(text)

    def mode(self):
        """'d--755' -> a complete i_mode."""
        spec = self.word("a mode")
        match = MODE_RE.fullmatch(spec)
        if not match:
            raise self.error("bad mode %r: expected [-bcd][u-][g-] then three "
                             "octal digits, as in 'd--755'" % spec)
        kind, setuid, setgid, perms = match.groups()
        return (IALLOC | TYPE_CHARS[kind] | int(perms, 8)
                | (ISUID if setuid == "u" else 0)
                | (ISGID if setgid == "g" else 0))

    def source_path(self, expected):
        """'$NAME/...' expands from the -D definitions; anything else is
        relative to the proto file's own directory."""
        text = self.word(expected).replace("\\", "/")
        if not text.startswith("$"):
            return self.path.parent / text
        name, _, rest = text[1:].partition("/")
        if name not in self.defines:
            raise self.error("undefined token $%s (pass -D %s=PATH)"
                             % (name, name))
        return Path(self.defines[name]) / rest

    def peek_dash(self):
        """Consume a lone '-' if that is the next word."""
        if self.pos < len(self.words) and self.words[self.pos][0] == "-":
            self.word("the boot sector file")
            return True
        return False

    def origin(self):
        return "%s line %d" % (self.path.name, self.line)

    def error(self, message):
        return MkfsError("%s: %s" % (self.origin(), message))


# --- the image ------------------------------------------------------------

class Image:
    """The disk image, built in memory and written out once at the end."""

    def __init__(self, nblocks):
        self.nblocks = nblocks
        self.data = bytearray(nblocks * BLOCK)

    def write(self, bno, block):
        """Write one block, zero padded if `block` is short."""
        if not 0 <= bno < self.nblocks:
            raise MkfsError("block %d is outside the %d-block image"
                            % (bno, self.nblocks))
        if len(block) > BLOCK:
            raise MkfsError("block %d: %d bytes do not fit in a %d-byte block"
                            % (bno, len(block), BLOCK))
        self.data[bno * BLOCK:(bno + 1) * BLOCK] = bytes(block).ljust(BLOCK, b"\0")

    def read(self, bno):
        return self.data[bno * BLOCK:(bno + 1) * BLOCK]

    def write_inode(self, ino, packed):
        """Inode n lives in block (n+31)/16, slot (n+31)%16."""
        slot = ino + INODES_PER_BLOCK * FIRST_INODE_BLOCK - 1
        start = (slot // INODES_PER_BLOCK * BLOCK
                 + slot % INODES_PER_BLOCK * INODE_SIZE)
        self.data[start:start + INODE_SIZE] = packed


class FreeList:
    """The V6 free list (bfree/alloc in the C): s_nfree and s_free[100] in the
    superblock, chained through the free blocks themselves.  A full s_free[]
    spills into the block being freed, which becomes the next link.  Block 0
    is freed first, so exhausting the list surfaces as take() returning 0
    rather than as an underflow.
    """

    def __init__(self, image):
        self.image = image
        self.nfree = 0
        self.free = [0] * FREE_PER_BLOCK

    def pack(self):
        """s_nfree then s_free[100] -- the layout of both the superblock field
        and every chain block."""
        return struct.pack("<%dH" % (FREE_PER_BLOCK + 1), self.nfree, *self.free)

    def give(self, bno):
        if self.nfree == FREE_PER_BLOCK:
            self.image.write(bno, self.pack())
            self.nfree = 0
        self.free[self.nfree] = bno
        self.nfree += 1

    def take(self):
        self.nfree -= 1
        bno, self.free[self.nfree] = self.free[self.nfree], 0
        if bno == 0:
            raise MkfsError("out of free space: the contents need more blocks "
                            "than fsize allows")
        if self.nfree == 0:                     # that was a chain block
            words = struct.unpack_from("<%dH" % (FREE_PER_BLOCK + 1),
                                       self.image.read(bno))
            self.nfree, self.free = words[0], list(words[1:])
        return bno

    def fill(self, fsize, isize):
        """Free the data blocks top down, so they are handed out ascending."""
        self.give(0)
        for bno in range(fsize - 1, isize + FIRST_INODE_BLOCK - 1, -1):
            self.give(bno)


# --- writing --------------------------------------------------------------

def write_content(content, image, freelist):
    """Copy `content` into fresh blocks.  Returns i_addr[] and the ILARG flag:
    8 block numbers fit in the inode, more need an indirect block."""
    blocks = []
    for start in range(0, len(content), BLOCK):
        bno = freelist.take()
        image.write(bno, content[start:start + BLOCK])
        blocks.append(bno)
    if len(blocks) <= DIRECT_ADDRS:
        return blocks + [0] * (DIRECT_ADDRS - len(blocks)), 0
    if len(blocks) > ADDRS_PER_BLOCK:
        raise MkfsError("%d blocks is more than one indirect block can address"
                        % len(blocks))
    indirect = freelist.take()
    image.write(indirect, struct.pack("<%dH" % len(blocks), *blocks))
    return [indirect] + [0] * (DIRECT_ADDRS - 1), ILARG


def write_node(node, image, freelist):
    """Write one entry's data blocks and its inode."""
    if node.type in (FileType.BLOCK, FileType.CHAR):
        major, minor = node.dev                 # no data: i_addr[0] is the dev
        size, large = 0, 0
        addrs = [major << 8 | minor] + [0] * (DIRECT_ADDRS - 1)
    else:
        content = node.content()
        size = len(content)
        addrs, large = write_content(content, image, freelist)
    # mode, nlink, uid, gid, the size split as V6 splits it (high byte in
    # i_size0, low word in i_size1), i_addr[8], atime, mtime.  32 bytes.
    image.write_inode(node.ino, struct.pack(
        "<HBBBBH8H4H", node.mode | large, node.nlink, node.uid, node.gid,
        size >> 16 & 0xFF, size & 0xFFFF, *addrs, 0, 0, 0, 0))


def superblock(fsize, isize, freelist):
    """The rest of the block -- s_inode[100], the lock and modified flags,
    s_time -- stays zero: an empty inode cache and a clean filesystem."""
    return (struct.pack("<HH", isize, fsize) + freelist.pack()
            + struct.pack("<H", 0))             # s_ninode


def build(proto_path, image_path, defines, verbose=False, offset=0):
    """Build the filesystem described by `proto_path`.  Returns the entry count.

    `offset` places it that many blocks into `image_path` instead of at the
    start, which is how a second rk unit is written into the same file: the
    rk driver maps unit N to block N*NRKBLK of the one IDE disk, so rk1 is
    just this image at offset 4872.  A non-zero offset patches the file in
    place and leaves everything outside its own range alone."""
    boot, (fsize, isize, swap), root = ProtoParser(proto_path, defines).parse()
    nodes = list(preorder(root))
    for ino, node in enumerate(nodes, 1):
        node.ino = ino

    if isize > fsize - isize - FIRST_INODE_BLOCK:
        raise MkfsError("bad geometry %d/%d: the inode blocks must leave at "
                        "least as many data blocks" % (fsize, isize))
    if len(nodes) > isize * INODES_PER_BLOCK:
        raise MkfsError("%d entries need more than the %d inodes isize=%d "
                        "provides" % (len(nodes), isize * INODES_PER_BLOCK, isize))
    image = Image(fsize + swap)
    if boot is not None:
        try:
            image.write(0, boot.read_bytes()[:BLOCK])
        except OSError as exc:
            raise MkfsError("cannot read boot sector %s: %s"
                            % (boot, exc.strerror))
    freelist = FreeList(image)
    freelist.fill(fsize, isize)
    for node in nodes:
        write_node(node, image, freelist)
    image.write(1, superblock(fsize, isize, freelist))

    if offset:
        path = Path(image_path)
        data = bytearray(path.read_bytes()) if path.exists() else bytearray()
        end = (offset + fsize + swap) * BLOCK
        data.extend(b"\0" * (end - len(data)))
        data[offset * BLOCK:end] = image.data
        path.write_bytes(data)
    else:
        Path(image_path).write_bytes(image.data)
    if verbose:
        print("%s%s: %d blocks, %d inode blocks, %d swap, %d entries"
              % (image_path, "+%d" % offset if offset else "",
                 fsize, isize, swap, len(nodes)))
    return len(nodes)


def main():
    def definition(text):
        name, sep, value = text.partition("=")
        if not sep:
            raise argparse.ArgumentTypeError("expected NAME=PATH, got %r" % text)
        return name, value

    ap = argparse.ArgumentParser(
        description="Build a Unix V6 filesystem image from a mkfs proto file.")
    ap.add_argument("image", help="output image file")
    ap.add_argument("proto", help="proto file describing the contents")
    ap.add_argument("-D", "--define", action="append", default=[],
                    type=definition, metavar="NAME=PATH",
                    help="expand $NAME in proto source paths to PATH")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--offset", type=int, default=0, metavar="BLOCKS",
                    help="write the filesystem this many blocks into the "
                         "image, patching it in place (rk unit N = N*4872)")
    args = ap.parse_args()

    try:
        build(args.proto, args.image, dict(args.define), args.verbose,
              args.offset)
    except MkfsError as exc:
        sys.exit("mkfs: %s" % exc)


if __name__ == "__main__":
    main()
