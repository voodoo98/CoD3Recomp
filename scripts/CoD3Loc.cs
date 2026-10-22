// CoD3 (Xbox 360) localized string banks: reading them out of the .cod archives
// and writing them back. Used by extract_subtitles.ps1 and pack_subtitles.ps1.
//
// Every .cod holds its localized strings ("Global.en.stbb" and the like) as a
// block that starts with "STRT". Layout (big endian, offsets from the block):
//
//   0x00  "STRT", 1.0f, hash mask, 0x00800010, record count, 0x8000xxxx, size
//   0x1C  hash table: `mask` pairs of (name hash, record index)
//   arr   record count x u32: flag 0x400000 (0xC00000 on the last slot) | the
//         offset of the record from the slot itself
//   ...   records: key (name hash), 0, W, then the NUL terminated text padded
//         to 4 bytes. W = (3 + text words) << 22 | 4. A record whose W does
//         not end in 0004 has no text of its own: W is a signed offset (20
//         bits) from the W field to the text of an earlier record
//   end   n, size + 8, then n words (0x0B, 0x0C and, when n is 3, arr); the
//         asset is `size + 8 + 4n` bytes
//
// The block is also listed in the archive's header, which names the asset's
// size in a handful of places (the pool extent table, the asset table, the
// name table); the new size is written over every one of them.
//
// The copy that counts is the one at the very end of the file. Some level
// archives hold an older copy further up, in the middle of the streamed data;
// that one is left alone.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace CoD3Loc
{
    public sealed class Record
    {
        public uint Key;
        public string Text;     // decoded text, a record that shared another's text has it copied in
        public bool Shared;
    }

    public sealed class StrtBlock
    {
        public long Offset;
        public uint[] Header = new uint[7];
        public byte[] HashTable;            // bytes 0x1C .. ArrayOffset
        public int ArrayOffset;
        public uint[] ArrayFlags;           // the top bits of each slot
        public uint[] Footer;               // the words after "size + 8": 0x0B, 0x0C and (when there are three) the array offset
        public List<Record> Records = new List<Record>();
        public int Size { get { return (int)Header[6]; } }
        public int AssetSize { get { return Size + 8 + 4 * Footer.Length; } }
        public long End { get { return Offset + AssetSize; } }
    }

    public static class Codec
    {
        public static readonly Encoding Text = Encoding.GetEncoding(1252);

        static uint BE(byte[] d, long p)
        {
            return (uint)(d[p] << 24 | d[p + 1] << 16 | d[p + 2] << 8 | d[p + 3]);
        }

        static void PutBE(byte[] d, long p, uint v)
        {
            d[p] = (byte)(v >> 24); d[p + 1] = (byte)(v >> 16); d[p + 2] = (byte)(v >> 8); d[p + 3] = (byte)v;
        }

        static uint LE(byte[] d, long p)
        {
            return (uint)(d[p] | d[p + 1] << 8 | d[p + 2] << 16 | d[p + 3] << 24);
        }

        static void PutLE(byte[] d, long p, uint v)
        {
            d[p] = (byte)v; d[p + 1] = (byte)(v >> 8); d[p + 2] = (byte)(v >> 16); d[p + 3] = (byte)(v >> 24);
        }

        static string CString(byte[] d, long p, long limit)
        {
            long e = p;
            while (e < limit && d[e] != 0) e++;
            if (e >= limit) throw new InvalidDataException("unterminated string at 0x" + p.ToString("X"));
            return Text.GetString(d, (int)p, (int)(e - p));
        }

        // Every well formed STRT block in the file, in file order.
        public static List<StrtBlock> Find(byte[] d, List<string> warnings)
        {
            var found = new List<StrtBlock>();
            for (long o = 0; o + 0x40 <= d.Length; o += 0x800)
            {
                if (d[o] != 'S' || d[o + 1] != 'T' || d[o + 2] != 'R' || d[o + 3] != 'T') continue;
                if (BE(d, o + 4) != 0x3F800000) continue;
                try
                {
                    found.Add(Parse(d, o));
                }
                catch (Exception e)
                {
                    if (warnings != null) warnings.Add("block at 0x" + o.ToString("X") + " skipped: " + e.Message);
                }
            }
            return found;
        }

        static StrtBlock Parse(byte[] d, long o)
        {
            var b = new StrtBlock { Offset = o };
            for (int i = 0; i < 7; i++) b.Header[i] = BE(d, o + 4 * i);
            uint mask = b.Header[2], count = b.Header[4];
            long size = b.Header[6];
            if (mask == 0 || (mask & (mask + 1)) != 0) throw new InvalidDataException("bad hash mask");
            if (o + size + 8 > d.Length) throw new InvalidDataException("block runs past the end of the file");
            uint words = BE(d, o + size);
            if (words < 2 || words > 4 || BE(d, o + size + 4) != size + 8 || o + size + 8 + 4 * words > d.Length) throw new InvalidDataException("bad footer");
            b.Footer = new uint[words];
            for (int i = 0; i < words; i++) b.Footer[i] = BE(d, o + size + 8 + 4 * i);
            b.ArrayOffset = 0x1C + 8 * (int)mask;
            if (words == 3 && b.Footer[2] != b.ArrayOffset) throw new InvalidDataException("array offset does not follow the hash table");
            if (b.ArrayOffset + 4L * count > size) throw new InvalidDataException("array past the end");
            b.HashTable = new byte[b.ArrayOffset - 0x1C];
            Array.Copy(d, o + 0x1C, b.HashTable, 0, b.HashTable.Length);
            b.ArrayFlags = new uint[count];

            long limit = o + size;
            for (int i = 0; i < count; i++)
            {
                long slot = o + b.ArrayOffset + 4L * i;
                uint v = BE(d, slot);
                b.ArrayFlags[i] = v & 0xFFC00000;
                long rp = slot + (v & 0x3FFFFF);
                if (rp < o || rp + 12 > limit) throw new InvalidDataException("record " + i + " outside the block");
                var r = new Record { Key = BE(d, rp) };
                uint w = BE(d, rp + 8);
                if ((w & 0xFFFF) == 4)
                {
                    r.Text = CString(d, rp + 12, limit);
                }
                else
                {
                    long off = (int)(w << 12) >> 12;
                    long tp = rp + 8 + off;
                    if (tp < o || tp >= limit) throw new InvalidDataException("record " + i + " shares text outside the block");
                    r.Text = CString(d, tp, limit);
                    r.Shared = true;
                }
                b.Records.Add(r);
            }
            return b;
        }

        // The block with these texts (one per record, same order) in place of
        // the original ones. Every record gets its own text.
        public static byte[] Build(StrtBlock b, IList<byte[]> texts)
        {
            int n = b.Records.Count;
            int recordsAt = b.ArrayOffset + 4 * n;
            int total = recordsAt;
            var starts = new int[n];
            for (int i = 0; i < n; i++)
            {
                starts[i] = total;
                total += 12 + ((texts[i].Length + 4) & ~3);
            }
            int size = total;
            var o = new byte[size + 8 + 4 * b.Footer.Length];
            for (int i = 0; i < 7; i++) PutBE(o, 4 * i, b.Header[i]);
            PutBE(o, 24, (uint)size);
            Array.Copy(b.HashTable, 0, o, 0x1C, b.HashTable.Length);
            for (int i = 0; i < n; i++)
            {
                int slot = b.ArrayOffset + 4 * i;
                PutBE(o, slot, b.ArrayFlags[i] | (uint)(starts[i] - slot));
                int p = starts[i];
                int words = 3 + ((texts[i].Length + 4) >> 2);
                if (words > 1023) throw new InvalidDataException("text too long (" + texts[i].Length + " bytes): " + b.Records[i].Key.ToString("x8"));
                PutBE(o, p, b.Records[i].Key);
                PutBE(o, p + 8, (uint)words << 22 | 4);
                Array.Copy(texts[i], 0, o, p + 12, texts[i].Length);
            }
            PutBE(o, size, (uint)b.Footer.Length);
            PutBE(o, size + 4, (uint)size + 8);
            for (int i = 0; i < b.Footer.Length; i++) PutBE(o, size + 8 + 4 * i, b.Footer[i]);
            return o;
        }

        // The last block of the file, if nothing but zeros follows it.
        public static StrtBlock Tail(byte[] d, List<StrtBlock> blocks)
        {
            if (blocks.Count == 0) return null;
            var t = blocks[blocks.Count - 1];
            for (long i = t.End; i < d.Length; i++) if (d[i] != 0) return null;
            return t;
        }

        // Writes `newSize` over every little endian word in the header that
        // holds the old asset size.
        public static int PatchHeader(byte[] d, uint oldSize, uint newSize)
        {
            long end = LE(d, 0x18);
            int hits = 0;
            for (long p = 0; p + 4 <= end; p += 4)
            {
                if (LE(d, p) != oldSize) continue;
                PutLE(d, p, newSize);
                hits++;
            }
            return hits;
        }

        static readonly Regex Timing = new Regex(@"^(\d*\.?\d+\|(?:\d*\.?\d+\|)?)(.*)$", RegexOptions.Singleline);

        // A subtitle line starts with "start|end|" (or "duration|"); that part is not for the translator.
        public static void Split(string full, out string prefix, out string body)
        {
            var m = Timing.Match(full);
            if (m.Success) { prefix = m.Groups[1].Value; body = m.Groups[2].Value; }
            else { prefix = ""; body = full; }
        }

        // Latin letters the Windows-1252 text of the title cannot show become the nearest it can.
        public static string Fit(string s, ref int replaced)
        {
            var sb = new StringBuilder(s.Length);
            foreach (char c in s)
            {
                switch (c)
                {
                    case 'ő': sb.Append('ö'); replaced++; break;
                    case 'Ő': sb.Append('Ö'); replaced++; break;
                    case 'ű': sb.Append('ü'); replaced++; break;
                    case 'Ű': sb.Append('Ü'); replaced++; break;
                    default: sb.Append(c); break;
                }
            }
            return sb.ToString();
        }

        // The native host's hu-double-acute-v1 font patch reserves four
        // otherwise unused CP1252 slots for Hungarian letters (CP1250 values).
        // The other CP1252 letters, including French place names, stay intact.
        public static string FitHungarian(string s)
        {
            if (s.IndexOfAny(new char[] { 'Õ', 'õ', 'Û', 'û', 'Ò', 'ò', 'Ù', 'ù' }) >= 0)
                throw new InvalidDataException("Hungarian font slots conflict with a translated character: " + s);
            return s.Replace('Ő', 'Õ').Replace('ő', 'õ').Replace('Ű', 'Û').Replace('ű', 'û');
        }

        public static string RelPath(string root, string path)
        {
            string r = Path.GetFullPath(root).TrimEnd('\\', '/') + "\\";
            string p = Path.GetFullPath(path);
            return (p.StartsWith(r, StringComparison.OrdinalIgnoreCase) ? p.Substring(r.Length) : p).Replace('\\', '/');
        }
    }

    // A small JSON reader/writer: strings, numbers, booleans, null, arrays, objects.
    public static class Json
    {
        public static string Quote(string s)
        {
            var sb = new StringBuilder("\"");
            foreach (char c in s)
            {
                switch (c)
                {
                    case '"': sb.Append("\\\""); break;
                    case '\\': sb.Append("\\\\"); break;
                    case '\n': sb.Append("\\n"); break;
                    case '\r': sb.Append("\\r"); break;
                    case '\t': sb.Append("\\t"); break;
                    default:
                        if (c < 0x20 || (c >= 0x7F && c < 0xA0)) sb.Append("\\u").Append(((int)c).ToString("x4"));
                        else sb.Append(c);
                        break;
                }
            }
            return sb.Append('"').ToString();
        }

        public static object Parse(string s)
        {
            int i = 0;
            object v = Value(s, ref i);
            Skip(s, ref i);
            if (i != s.Length) throw new FormatException("trailing characters at " + i);
            return v;
        }

        static void Skip(string s, ref int i)
        {
            while (i < s.Length && char.IsWhiteSpace(s[i])) i++;
        }

        static object Value(string s, ref int i)
        {
            Skip(s, ref i);
            if (i >= s.Length) throw new FormatException("unexpected end");
            char c = s[i];
            if (c == '{')
            {
                var d = new Dictionary<string, object>();
                i++; Skip(s, ref i);
                if (s[i] == '}') { i++; return d; }
                while (true)
                {
                    Skip(s, ref i);
                    string k = Str(s, ref i);
                    Skip(s, ref i);
                    if (s[i++] != ':') throw new FormatException("':' expected at " + i);
                    d[k] = Value(s, ref i);
                    Skip(s, ref i);
                    if (s[i] == ',') { i++; continue; }
                    if (s[i] == '}') { i++; return d; }
                    throw new FormatException("',' or '}' expected at " + i);
                }
            }
            if (c == '[')
            {
                var l = new List<object>();
                i++; Skip(s, ref i);
                if (s[i] == ']') { i++; return l; }
                while (true)
                {
                    l.Add(Value(s, ref i));
                    Skip(s, ref i);
                    if (s[i] == ',') { i++; continue; }
                    if (s[i] == ']') { i++; return l; }
                    throw new FormatException("',' or ']' expected at " + i);
                }
            }
            if (c == '"') return Str(s, ref i);
            if (string.CompareOrdinal(s, i, "true", 0, 4) == 0) { i += 4; return true; }
            if (string.CompareOrdinal(s, i, "false", 0, 5) == 0) { i += 5; return false; }
            if (string.CompareOrdinal(s, i, "null", 0, 4) == 0) { i += 4; return null; }
            int st = i;
            while (i < s.Length && "+-0123456789.eE".IndexOf(s[i]) >= 0) i++;
            if (st == i) throw new FormatException("unexpected '" + c + "' at " + i);
            return double.Parse(s.Substring(st, i - st), CultureInfo.InvariantCulture);
        }

        static string Str(string s, ref int i)
        {
            if (s[i] != '"') throw new FormatException("string expected at " + i);
            i++;
            var sb = new StringBuilder();
            while (true)
            {
                if (i >= s.Length) throw new FormatException("unterminated string");
                char c = s[i++];
                if (c == '"') return sb.ToString();
                if (c != '\\') { sb.Append(c); continue; }
                c = s[i++];
                switch (c)
                {
                    case 'n': sb.Append('\n'); break;
                    case 'r': sb.Append('\r'); break;
                    case 't': sb.Append('\t'); break;
                    case 'b': sb.Append('\b'); break;
                    case 'f': sb.Append('\f'); break;
                    case 'u': sb.Append((char)int.Parse(s.Substring(i, 4), NumberStyles.HexNumber)); i += 4; break;
                    default: sb.Append(c); break;
                }
            }
        }
    }

    public sealed class Entry
    {
        public string Id, Prefix, En, Hu;
        public List<string> Files = new List<string>();
        public string Full { get { return Prefix + En; } }
    }

    public static class Tool
    {
        static IEnumerable<string> Archives(string game)
        {
            var list = new List<string>(Directory.GetFiles(game, "*.cod", SearchOption.AllDirectories));
            list.Sort(StringComparer.OrdinalIgnoreCase);
            return list;
        }

        // Writes the strings of every archive under `game` to `outDir`, one
        // entry per distinct (name hash, text); `chunk` > 0 splits the list
        // into files of that many entries.
        public static void Extract(string game, string outDir, int chunk, string scope)
        {
            var entries = new List<Entry>();
            var index = new Dictionary<string, Entry>();
            var high = new SortedDictionary<int, int>();
            int archives = 0, ignored = 0, records = 0;
            foreach (string path in Archives(game))
            {
                string rel = Codec.RelPath(game, path);
                if (scope != "all" && !rel.StartsWith(scope + "/", StringComparison.OrdinalIgnoreCase)) continue;
                var warnings = new List<string>();
                byte[] d = File.ReadAllBytes(path);
                var blocks = Codec.Find(d, warnings);
                foreach (string w in warnings) Console.WriteLine("  warning: " + rel + ": " + w);
                var tail = Codec.Tail(d, blocks);
                if (tail == null)
                {
                    if (blocks.Count > 0) Console.WriteLine("  warning: " + rel + ": no string bank at the end of the file, skipped");
                    continue;
                }
                archives++;
                ignored += blocks.Count - 1;
                foreach (var r in tail.Records)
                {
                    records++;
                    string id = r.Key.ToString("x8");
                    string k = id + "\u0001" + r.Text;
                    Entry e;
                    if (!index.TryGetValue(k, out e))
                    {
                        e = new Entry { Id = id, Hu = "" };
                        Codec.Split(r.Text, out e.Prefix, out e.En);
                        index[k] = e;
                        entries.Add(e);
                        foreach (char c in e.Full) if (c >= 0x80) { int n; high.TryGetValue(c, out n); high[c] = n + 1; }
                    }
                    if (!e.Files.Contains(rel)) e.Files.Add(rel);
                }
                Console.WriteLine("  " + rel + ": " + tail.Records.Count + " strings");
            }
            Directory.CreateDirectory(outDir);
            foreach (string old in Directory.GetFiles(outDir, "strings*.json")) File.Delete(old);
            int per = chunk > 0 ? chunk : int.MaxValue;
            int parts = Math.Max(1, (entries.Count + per - 1) / per);
            for (int part = 0; part < parts; part++)
            {
                string name = parts == 1 ? "strings.json" : "strings_" + (part + 1).ToString("000") + ".json";
                using (var w = new StreamWriter(Path.Combine(outDir, name), false, new UTF8Encoding(false)))
                {
                    w.Write("{\n\"format\": \"cod3-strings/1\",\n\"entries\": [\n");
                    int from = part * per, to = Math.Min(entries.Count, from + per);
                    for (int i = from; i < to; i++)
                    {
                        var e = entries[i];
                        w.Write("{\"id\": " + Json.Quote(e.Id));
                        if (e.Prefix.Length > 0) w.Write(", \"t\": " + Json.Quote(e.Prefix));
                        w.Write(", \"en\": " + Json.Quote(e.En) + ", \"hu\": \"\", \"files\": [");
                        for (int f = 0; f < e.Files.Count; f++) w.Write((f > 0 ? ", " : "") + Json.Quote(e.Files[f]));
                        w.Write("]}" + (i + 1 < to ? ",\n" : "\n"));
                    }
                    w.Write("]\n}\n");
                }
            }
            Console.WriteLine();
            Console.WriteLine(archives + " archives, " + records + " strings, " + entries.Count + " distinct, written to " + outDir + " in " + parts + " file(s)");
            if (ignored > 0) Console.WriteLine(ignored + " older string bank copies in the middle of level archives were left out (the engine uses the one at the end of the file)");
            if (high.Count > 0)
            {
                var sb = new StringBuilder();
                foreach (var kv in high) sb.Append("U+" + kv.Key.ToString("X4") + "(" + kv.Value + ") ");
                Console.WriteLine("characters above 0x7F in the English text: " + sb);
            }
        }

        static void LoadEntries(string path, Dictionary<string, Entry> into, ref int translated)
        {
            var root = (Dictionary<string, object>)Json.Parse(File.ReadAllText(path, Encoding.UTF8));
            foreach (Dictionary<string, object> o in (List<object>)root["entries"])
            {
                var e = new Entry();
                e.Id = (string)o["id"];
                e.Prefix = o.ContainsKey("t") ? (string)o["t"] : "";
                e.En = (string)o["en"];
                e.Hu = o.ContainsKey("hu") && o["hu"] != null ? (string)o["hu"] : "";
                if (e.Hu.Length == 0) continue;
                into[e.Id + "\u0001" + e.Full] = e;
                translated++;
            }
        }

        // Rewrites the string banks of the archives under `game` with the
        // translated entries from `strings` (a file, or a folder of strings*.json)
        // and puts the changed archives in `outDir`, same relative paths.
        public static void Pack(string game, string strings, string outDir, string scope)
        {
            Pack(game, strings, outDir, scope, false);
        }

        public static void Pack(string game, string strings, string outDir, string scope, bool hungarianAccents)
        {
            var table = new Dictionary<string, Entry>();
            int translated = 0;
            if (Directory.Exists(strings))
            {
                foreach (string f in Directory.GetFiles(strings, "strings*.json")) LoadEntries(f, table, ref translated);
            }
            else LoadEntries(strings, table, ref translated);
            Console.WriteLine(translated + " translated entries read");
            if (translated == 0) throw new InvalidOperationException("nothing is translated yet (every \"hu\" is empty)");

            int files = 0, strs = 0, subst = 0, over = 0;
            foreach (string path in Archives(game))
            {
                string rel = Codec.RelPath(game, path);
                if (scope != "all" && !rel.StartsWith(scope + "/", StringComparison.OrdinalIgnoreCase)) continue;
                byte[] d = File.ReadAllBytes(path);
                var blocks = Codec.Find(d, null);
                var tail = Codec.Tail(d, blocks);
                if (tail == null) continue;

                var texts = new List<byte[]>();
                int changed = 0;
                foreach (var r in tail.Records)
                {
                    string full = r.Text;
                    Entry e;
                    if (table.TryGetValue(r.Key.ToString("x8") + "\u0001" + full, out e))
                    {
                        string prefix, body;
                        Codec.Split(full, out prefix, out body);
                        full = prefix + (hungarianAccents ? Codec.FitHungarian(e.Hu) : Codec.Fit(e.Hu, ref subst));
                        changed++;
                    }
                    byte[] bytes = Codec.Text.GetBytes(full);
                    for (int i = 0; i < bytes.Length; i++) if (bytes[i] == '?' && full[i] != '?') { over++; break; }
                    texts.Add(bytes);
                }
                if (changed == 0) continue;

                byte[] block = Codec.Build(tail, texts);
                long fileEnd = tail.Offset + block.Length;
                fileEnd = (fileEnd + 0xFFFF) & ~0xFFFFL;
                byte[] n = new byte[fileEnd];
                Array.Copy(d, 0, n, 0, tail.Offset);
                Array.Copy(block, 0, n, tail.Offset, block.Length);
                int hits = Codec.PatchHeader(n, (uint)tail.AssetSize, (uint)block.Length);
                if (hits < 3) throw new InvalidDataException(rel + ": the asset size was found only " + hits + " times in the header");

                string dest = Path.Combine(outDir, rel.Replace('/', '\\'));
                Directory.CreateDirectory(Path.GetDirectoryName(dest));
                File.WriteAllBytes(dest, n);
                files++; strs += changed;
                Console.WriteLine("  " + rel + ": " + changed + " strings, block " + tail.AssetSize + " -> " + block.Length + " bytes, file " + d.Length + " -> " + n.Length);
            }
            Console.WriteLine();
            Console.WriteLine(files + " archives written to " + outDir + ", " + strs + " strings replaced");
            if (subst > 0) Console.WriteLine(subst + " characters (ő ű and capitals) are not in the title's font encoding and were written as ö ü");
            if (over > 0) Console.WriteLine("warning: " + over + " strings hold characters Windows-1252 cannot hold; they were written as '?'");
        }
    }
}
