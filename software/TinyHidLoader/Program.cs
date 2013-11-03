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
            if (args.Length == 1 && File.Exists(args[0]))
            {
                HexFile file = new HexFile(args[0]);
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

                byte[] programm = new byte[Loader.LOADERSTART];
                for (int i = 0; i < programm.Length; i++) programm[i] = 0xff;
                file.Fill(programm);
                try
                {
                    DateTime now = DateTime.Now;
                    ldr.WriteFlash(programm, 0);
                    int ellapsed = (int)(DateTime.Now - now).TotalMilliseconds;
                    Console.WriteLine("Done in {0} ms", ellapsed);
                }
                catch (Exception e)
                {
                    Console.WriteLine(e.Message);
                    return;
                }
                ldr.LeaveBootloader();
                Console.WriteLine("Success");
                return;
            }
            Console.WriteLine("USE: TinyHidLoader.exe FILE.HEX");
        }
    }
}
