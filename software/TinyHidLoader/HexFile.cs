using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DeliSu
{
    public class HexFile
    {
        public class HexChunk
        {
            public long Offset { get; set; }
            public byte[] Data { get; set; }

            public HexChunk(long offset, byte[] data)
            {
                this.Offset = offset;
                this.Data = data;
            }

            public bool Intersect(HexChunk other)
            {
                return Offset <= other.Offset + other.Data.Length && other.Offset <= Offset + Data.Length;
            }

            public void Combine(HexChunk other)
            {
                long start = Math.Min(Offset, other.Offset);
                long end = Math.Max(Offset + Data.Length, other.Offset + other.Data.Length);
                int length = (int)(end - start);
                byte[] buffer = new byte[length];
                int ss = (int)(Offset - start);
                for (int i = 0; i < Data.Length; i++)
                {
                    buffer[ss + i] = Data[i];
                }
                ss = (int)(other.Offset - start);
                for (int i = 0; i < other.Data.Length; i++)
                {
                    buffer[ss + i] = other.Data[i];
                }
                Offset = start;
                Data = buffer;
            }
        }

        public List<HexChunk> Chunks { get; set; }

        public HexFile(string filename)
        {
            Chunks = new List<HexChunk>();
            Open(filename);
        }

        public HexFile()
        {
            Chunks = new List<HexChunk>();
        }

        private void AppendData(long offset, byte[] data)
        {
            HexChunk nchunk = new HexChunk(offset, data);
            if (Chunks.Count > 0)
            {
                HexChunk lchunk = Chunks[Chunks.Count - 1];
                if (lchunk.Intersect(nchunk))
                {
                    lchunk.Combine(nchunk);
                    return;
                }
                for (int i = 0; i < Chunks.Count; i++)
                {
                    if (Chunks[i].Intersect(nchunk))
                    {
                        Chunks[i].Combine(nchunk);
                        return;
                    }
                }
            }
            Chunks.Add(nchunk);
        }

        public void Fill(byte[] buffer)
        {
            foreach (HexFile.HexChunk c in Chunks)
            {
                long pos = c.Offset;
                for (int i = 0; i < c.Data.Length; i++)
                {
                    buffer[pos++] = c.Data[i];
                }
            }
        }

        public void Open(string filename)
        {
            long currentOffset = 0;
            int ln = 0;
            bool hasEnd = false;
            foreach (string line in File.ReadAllLines(filename))
            {
                if(string.IsNullOrWhiteSpace(line)) continue;
                if (hasEnd) throw new FormatException("Dataline after end at " + ln + ": " + line);

                if (line.Length < 11) throw new FormatException("Invalid line, too short at " + ln + ": " + line);
                int length = int.Parse(line.Substring(1, 2), NumberStyles.HexNumber);
                if (line.Trim().Length != 11 + length * 2) throw new FormatException("Invalid line, invalid length at " + ln + ": " + line);
                
                int address = int.Parse(line.Substring(3, 4), NumberStyles.HexNumber);
                int type = int.Parse(line.Substring(7, 2), NumberStyles.HexNumber);
                int summ = length + (address & 0xff) + (address >> 8 & 0xff) + type;


                byte[] data = new byte[length];

                for (int i = 0; i < length; i++)
                {
                    data[i] = byte.Parse(line.Substring(9 + i * 2, 2), NumberStyles.HexNumber);
                    summ += data[i];
                }
                summ += byte.Parse(line.Substring(9 + length * 2, 2), NumberStyles.HexNumber);
                if ((summ & 0xff) != 0) throw new FormatException("Invalid line, invalid checksum at " + ln + ": " + line);

                if (type == 1)
                {
                    hasEnd = true;
                }
                else if(type == 2)
                {
                    currentOffset = (data[0] << 12) | (data[1] << 4);
                }
                else if (type == 4)
                {
                    currentOffset = (long)(data[0] << 24) | (long)(data[1] << 16);
                }
                else if (type == 0)
                {
                    AppendData(currentOffset + address, data);
                }
                else
                {
                    throw new FormatException("Invalid directive " + type + " at " + ln);
                }

                ln++;
            }
            if (!hasEnd)
            {
                throw new FormatException("No file end directive found");
            }
        }

        public void Write(string filename)
        {
            using (StreamWriter writer = new StreamWriter(filename))
            {
                foreach (HexChunk chunk in Chunks)
                {
                    byte[] buffer = chunk.Data;
                    int rest = buffer.Length;
                    int address = (int)chunk.Offset;
                    for (int i = 0; i < buffer.Length; i += 16)
                    {
                        int length = rest > 16 ? 16 : rest;
                        byte cycl = (byte)length;
                        writer.Write(":{0:X2}{1:X4}00", length, address);
                        cycl = (byte)(cycl + (address & 0xff));
                        cycl = (byte)(cycl + ((address >> 8) & 0xff));
                        for (int j = 0; j < length; j++)
                        {
                            writer.Write("{0:X2}", buffer[i + j]);
                            cycl += buffer[i + j];
                        }
                        writer.Write("{0:X2}", (byte)(0x100 - cycl));
                        address += 16;
                        rest -= 16;
                        writer.WriteLine();
                    }
                }
                writer.WriteLine(":00000001FF");
            }
        }
    }
}
