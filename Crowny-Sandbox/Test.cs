using System;
using Crowny;

namespace Sandbox
{
    class Test
    {

        public static void Main(string[] args)
        {
            Vector3 position = new Vector3(1f, 2f, 3f);
            Console.WriteLine(Vector3.Distance(new Vector3(1f, 1f, 1f), new Vector3(2f, 2f, 2f)));
            Console.WriteLine(position);
            Console.Read();
        }

    }
}
