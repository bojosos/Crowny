using System;

using Crowny;
using Crowny.Math;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {
        public void Start()
        {
            Debug.Log("Initialized");
        }

        public void Update()
        {
        	transform.position += new Vector3(0.1f, 0f, 0f) * Time.deltaTime;
        }
    }
}
