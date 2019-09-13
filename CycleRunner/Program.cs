using System;
using System.Diagnostics;

namespace CycleRunner
{
    internal class Program
    {
        public static int Main(string[] args)
        {
            var program = @"C:/work/NetRuntimeWaiter/Debug/NetRuntimeWaiter.exe";
            for (int i = 0; i < 100000; ++i)
            {
                Console.WriteLine("Try " + i);
                var startInfo = new ProcessStartInfo(program, args.Length > 0 && args[0] == "b" ? "b" : "a")
                {
                    UseShellExecute = false,
                    RedirectStandardOutput = true
                };
                var process = Process.Start(startInfo);
                string line = null;
                while ((line = process.StandardOutput.ReadLine()) != null)
                {
                    Console.WriteLine(line);
                }

                process.WaitForExit();
                if (process.ExitCode != 0)
                {
                    Console.WriteLine("ERROR CODE: " + process.ExitCode);
                    return 1;
                }
            }

            return 0;
        }
    }
}