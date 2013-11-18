using HidSharp;
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;

namespace DeliSu.TinyHidLoader
{
    [Flags]
    enum LoaderCommand : byte
    {
        ReadFlash = 0,
        WriteFlash = 0x10,
        EraseFlash = 0x20,
        EraseEeprom = 0x40,
        LeaveBootloader = 0x80,
    }

    public class Loader
    {
        public const int PAGESIZE = 64;
        public const int LOADERSTART = 0x1800 - 4;
        public const int FLASHSIZE = 0x2000;
        const int REPORT_SIZE = PAGESIZE + 3;
        const int REPORT_COMMAND = 1;
        const int REPORT_DATA = 3;
        const int REPORT_CRC = 2;
        HidDevice dev;

        public static HidDevice Find(int vid, int pid, string vendor, string device, int featureSize)
        {
            HidDeviceLoader ldr = new HidDeviceLoader();

            foreach (HidDevice dev in ldr.GetDevices())
            {
                if (vid != 0 && dev.VendorID != vid) continue;
                if (pid != 0 && dev.ProductID != pid) continue;
                if (vendor != null && dev.Manufacturer != vendor) continue;
                if (device != null && dev.ProductName != device) continue;
                if (featureSize != 0 && dev.MaxFeatureReportLength != featureSize) continue;
                return dev;
            }
            return null;
        }


        public static Loader TryGetLoader(int timeout)
        {
            for (int i = 0; ; i++)
            {
                try
                {
                    return new Loader();
                }
                catch
                {
                    if (i > timeout) throw;
                    Thread.Sleep(1000);
                }
            }
        }

        public static byte Crc8(byte[] buffer, int offset, int count)
        {
            byte crc = 0;
            count += offset;
            for (; offset < count; offset++)
            {
                crc = (byte)(crc ^ buffer[offset]);
                for (int i = 0; i < 8; i++)
                {
                    if ((crc & 0x01) != 0)
                        crc = (byte)((crc >> 1) ^ 0x8C);
                    else
                        crc >>= 1;
                }
            }

            return crc;
        }

        public static ushort Crc16(byte[] buffer, int offset, int count)
        {
            ushort crc = 0xffff;
            for(int j = 0; j < count; j++)
            {
                crc ^= buffer[offset++];
                for (int i = 0; i < 8; ++i)
                {
                    if ((crc & 1) != 0)
                        crc = (ushort)((crc >> 1) ^ 0xA001);
                    else
                        crc = (ushort)(crc >> 1);
                }
            }
	        return crc;
        }

        public Loader()
        {
            dev = Find(0x16c0, 0x05df, "deli.su", "TinyHID Loader", 67);
            if (dev == null) throw new Exception("Device not found");
        }

        /// <summary>
        /// Очищает весь EEPROM
        /// </summary>
        /// <remarks>
        /// Загрузчик должен быть скомпилирован с поддержкой очистки EEPROM
        /// </remarks>
        public void EraseEeprom()
        {
            using (HidStream stream = dev.Open())
            {
                byte[] buffer = new byte[67];
                buffer[REPORT_COMMAND] = (byte)LoaderCommand.EraseEeprom;
                buffer[REPORT_CRC] = Crc8(buffer, REPORT_DATA, PAGESIZE);
                stream.SetFeature(buffer);
            }
        }

        /// <summary>
        /// Очищает всё FLASH-память
        /// </summary>
        public void EraseFlash()
        {
            using (HidStream stream = dev.Open())
            {
                byte[] buffer = new byte[REPORT_SIZE];
                buffer[REPORT_COMMAND] = (byte)LoaderCommand.EraseFlash;
                buffer[REPORT_CRC] = Crc8(buffer, REPORT_DATA, PAGESIZE);
                stream.SetFeature(buffer);
            }
        }

        /// <summary>
        /// Выходит из режима загрузчика
        /// </summary>
        /// <remarks>
        /// Загрузчик должен быть скомпилирован с поддержкой выхода
        /// </remarks>
        public void LeaveBootloader()
        {
            using (HidStream stream = dev.Open())
            {
                byte[] buffer = new byte[REPORT_SIZE];
                buffer[REPORT_COMMAND] = (byte)LoaderCommand.LeaveBootloader;
                buffer[REPORT_CRC] = Crc8(buffer, REPORT_DATA, PAGESIZE);
                stream.SetFeature(buffer);
            }
        }

        /// <summary>
        /// Читает содержимое FLASH вплоть до бутлодера, но не вместе с ним
        /// </summary>
        /// <remarks>
        /// Бутлодеровскими также считаются последние 4 байта последней страницы данных перед ним
        /// </remarks>
        /// <remarks>
        /// Загрузчик должен быть скомпилирован с поддержкой чтения из Flash
        /// </remarks>
        /// <param name="programm">Образ прошивки</param>
        /// <param name="offset">Смещение в массиве образа</param>
        /// <returns>Количество прочтённых данных</returns>
        public int ReadFlash(byte[] programm, int offset)
        {
            using (HidStream stream = dev.Open())
            {
                byte[] buffer = new byte[REPORT_SIZE];
                buffer[REPORT_COMMAND] = (byte)LoaderCommand.ReadFlash;
                buffer[REPORT_CRC] = Crc8(buffer, REPORT_COMMAND, REPORT_CRC - REPORT_COMMAND);
                stream.SetFeature(buffer);
                int readed = 0;
                while(true)
                {
                    stream.GetFeature(buffer);
                    if (buffer[REPORT_CRC] != Crc8(buffer, 1, REPORT_CRC - REPORT_COMMAND))
                        throw new IOException("transfer fails, try again");

                    for (int i = REPORT_DATA; i < PAGESIZE + REPORT_DATA; i++)
                    {
                        programm[offset++] = buffer[i];
                        if (++readed >= LOADERSTART) return readed;
                    }
                }
            }
        }

        /// <summary>
        /// Читает содержимое FLASH вплоть до бутлодера, но не вместе с ним
        /// </summary>
        /// <remarks>
        /// Бутлодеровскими также считаются последние 4 байта последней страницы данных перед ним
        /// </remarks>
        /// <remarks>
        /// Загрузчик должен быть скомпилирован с поддержкой чтения из Flash
        /// </remarks>
        /// <param name="stream">Поток, в который считывается весь Bootloader</param>
        /// <returns>Количество прочитанных байт</returns>
        public int ReadFlash(Stream stream)
        {
            using (HidStream dstream = dev.Open())
            {
                byte[] buffer = new byte[REPORT_SIZE];
                buffer[REPORT_COMMAND] = (byte)LoaderCommand.ReadFlash;
                buffer[REPORT_CRC] = Crc8(buffer, 1, REPORT_CRC - REPORT_COMMAND);
                dstream.SetFeature(buffer);
                int readed = 0;
                while (true)
                {
                    dstream.GetFeature(buffer);
                    if (buffer[REPORT_CRC] != Crc8(buffer, 1, REPORT_CRC - REPORT_COMMAND))
                        throw new IOException("transfer fails, try again");

                    int rest = PAGESIZE;
                    if (readed + PAGESIZE >= LOADERSTART) rest = LOADERSTART - readed;
                    stream.Write(buffer, 2, rest);

                    readed += PAGESIZE;
                    if (readed >= LOADERSTART) return readed;
                }
            }
        }

        /// <summary>
        /// Пишет программу в flash
        /// </summary>
        /// <remarks>
        /// Бутлодеровскими также считаются последние 4 байта последней страницы данных перед ним
        /// </remarks>
        /// <param name="programm">Образ прошивки</param>
        /// <param name="offset">Смещение в массиве образа</param>
        /// <returns></returns>
        public int WriteFlash(byte[] programm, int offset)
        {
            using (HidStream stream = dev.Open())
            {
                byte[] buffer = new byte[REPORT_SIZE + 1];

                int writed = 0;
                while (true)
                {
                    buffer[1] = (byte)LoaderCommand.WriteFlash;
                    if (writed == 0) buffer[1] |= (byte)LoaderCommand.EraseFlash;

                    for (int i = REPORT_DATA; i < REPORT_DATA + PAGESIZE; i++)
                    {
                        if (++writed >= LOADERSTART)
                        {
                            buffer[i] = 0xff;
                            writed = LOADERSTART;
                        }
                        else
                        {
                            buffer[i] = programm[offset++];
                        }
                    }
                    buffer[REPORT_CRC] = Crc8(buffer, REPORT_DATA, PAGESIZE);
                    for (int i = 0; ; i++)
                    {
                        try
                        {
                            // Не факт, что устройство уже аклималось, или что USB контроллер его подхватил снова
                            // Так что возможны и вылеты. И раз они есть - то надо пробовать снова и снова.
                            stream.SetFeature(buffer);
                            break;
                        }
                        catch
                        {
                            if (i > 10) throw new Exception("can`t write at " + (writed - PAGESIZE));
                            Thread.Sleep(400);
                        }
                    }
                    if (buffer[REPORT_COMMAND] == (byte)LoaderCommand.WriteFlash)
                    {
                        Thread.Sleep(5);
                    }
                    else
                    {
                        Thread.Sleep(300);
                    }
                    if (writed >= LOADERSTART) return writed;
                }
            }
        }
    }
}
