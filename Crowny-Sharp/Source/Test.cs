using System;

using Crowny;
using Crowny.Math;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {
        public void Start()
        {
            Console.WriteLine("Calling start");
            Debug.Log("Test debug");
            Debug.Log(transform.position.ToString());
            transform.position += new Vector3(0.1f, 0f, 0f);// * Time.deltaTime;
        }

        public void After()
        {

        }

        public void Update()
        {
        	transform.position += new Vector3(0.1f, 0f, 0f);// * Time.deltaTime;
        }
    }
}
