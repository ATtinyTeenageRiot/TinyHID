using HidSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace DeliSu.TinyHidLoader
{
    class Program
    {
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

            if (args.Length == 1 && File.Exists(args[0]))
            {
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
            }
            Console.WriteLine("USE: TinyHidLoader.exe FILE.HEX");
        }
    }
}
