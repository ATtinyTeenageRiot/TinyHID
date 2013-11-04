using DeliSu.TinyHidLoader;
using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading;

namespace DeliSu.TinyLoaderCmd
{
    class Program
    {
        const int RELOADER_INFO = 0x0280;
        const int RELOADER_INFO_OFFSET = RELOADER_INFO;
        const int RELOADER_INFO_CRC = RELOADER_INFO + 2;
        const int RELOADER_BOOTLOADER_DATA = RELOADER_INFO + Loader.PAGESIZE;

        static void Main(string[] args)
        {
            Loader ldr;
            try
            {
                ldr = new Loader();
            }
            catch (Exception e)
            {
                Console.WriteLine(e.Message);
                return;
            }

            if (args.Length == 1 && File.Exists(args[0]) ||
                args.Length == 2 && args[1] == "notleave" && File.Exists(args[0]))
            {
                // Write
                HexFile file = new HexFile(args[0]);
                byte[] programm = new byte[Loader.LOADERSTART];
                for (int i = 0; i < programm.Length; i++) programm[i] = 0xff;
                file.Fill(programm);
                try
                {
                    DateTime now = DateTime.Now;
                    ldr.WriteFlash(programm, 0);
                    int ellapsed = (int)(DateTime.Now - now).TotalMilliseconds;
                    Console.WriteLine("Done in {0} ms", ellapsed);
                    if (args.Length != 2 || args[1] != "notleave")
                        ldr.LeaveBootloader();
                    Console.WriteLine("Success");
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                    return;
                }
                return;
            }
            else if (args.Length == 2 && args[0] == "reload" && File.Exists(args[1]))
            {
                string exe = new Uri(Assembly.GetExecutingAssembly().CodeBase).LocalPath;
                string reloader = Path.Combine(Path.GetDirectoryName(exe), "reloader.hex");
                if (!File.Exists(reloader))
                {
                    Console.WriteLine("reloader.hex not exists");
                    return;
                }
                // create image for writing
                byte[] programm = new byte[Loader.LOADERSTART];
                // fill image with 0xff
                for (int i = 0; i < programm.Length; i++) programm[i] = 0xff;
                // write jump to reloader
                programm[0] = 0x20;
                programm[1] = 0xc0;
                HexFile rel = new HexFile(reloader);
                // write reloader
                rel.Fill(programm);

                if (rel.End > RELOADER_INFO)
                {
                    Console.WriteLine("reloader too big");
                }

                HexFile hf = new HexFile(args[1]);
                long address = hf.Offset;
                long size = Loader.FLASHSIZE - address;
                if (size + RELOADER_BOOTLOADER_DATA > Loader.LOADERSTART)
                {
                    Console.WriteLine("bootloader is too big to fit");
                    return;
                }
                hf.Fill(programm, RELOADER_BOOTLOADER_DATA - address);
                programm[RELOADER_INFO_OFFSET] = (byte)(address & 0xff);
                programm[RELOADER_INFO_OFFSET + 1] = (byte)((address >> 8) & 0xff);
                ushort crc = Loader.Crc16(programm, RELOADER_BOOTLOADER_DATA, (int)size);
                programm[RELOADER_INFO_CRC] = (byte)(crc & 0xff);
                programm[RELOADER_INFO_CRC + 1] = (byte)((crc >> 8) & 0xff);
                HexFile ot = new HexFile();
                ot.Chunks.Add(new HexFile.HexChunk(0, programm));
                try
                {
                    Console.WriteLine("Writing new bootloader");
                    DateTime now = DateTime.Now;
                    ldr.WriteFlash(programm, 0);
                    int ellapsed = (int)(DateTime.Now - now).TotalMilliseconds;
                    Console.WriteLine("Done in {0} ms, begin update", ellapsed);
                    ldr.LeaveBootloader();
                    Thread.Sleep(5000);
                    ldr = Loader.TryGetLoader(40);
                    Console.WriteLine("Erasing empty space");
                    ldr.EraseFlash();
                    Console.WriteLine("Success");
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                    return;
                }
            }
            else if (args.Length == 2 && args[0] == "read")
            {
                byte[] programm = new byte[Loader.LOADERSTART];
                for (int i = 0; i < programm.Length; i++) programm[i] = 0xff;
                try
                {
                    DateTime now = DateTime.Now;
                    ldr.ReadFlash(programm, 0);
                    HexFile hf = new HexFile();
                    hf.Chunks.Add(new HexFile.HexChunk(0, programm));
                    hf.Write(args[1]);
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                    return;
                }
                return;
            }
            else if (args.Length == 2 && args[0] == "erase" && args[1] == "flash")
            {
                ldr.EraseFlash();
                return;
            }
            else if (args.Length == 2 && args[0] == "erase" && args[1] == "eeprom")
            {
                ldr.EraseEeprom();
                return;
            }
            Console.WriteLine("USE: TinyHidLoader.exe FILE.HEX - to write flash and exit to application");
            Console.WriteLine("USE: TinyHidLoader.exe FILE.HEX noleave - to write flash");
            Console.WriteLine("USE: TinyHidLoader.exe read FILE.HEX - to read flash");
            Console.WriteLine("USE: TinyHidLoader.exe erase flash - to erase flash");
            Console.WriteLine("USE: TinyHidLoader.exe erase eeprom - to erase eeprom");
        }
    }
}
