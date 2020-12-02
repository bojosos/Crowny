using System;

using Crowny;

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
            if (Input.GetKey(KeyCode.Left))
        	    transform.position += Vector3.left * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Right))
        	    transform.position += Vector3.right * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Up))
               transform.position += Vector3.up * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Down))
        	    transform.position += Vector3.down * Time.smoothDeltaTime;
        }
    }
}
