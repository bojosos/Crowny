using System;

using Crowny;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {

        [Crowny.Range(0.0f, 10.0f), ShowInInspector]
        public float speed = 1.0f;

        public void Start()
        {
            Debug.Log("Start called!");
            Debug.Log(transform.position);
        }

        public void Update()
        {
            if (Input.GetKey(KeyCode.Left))
        	    transform.position += Vector3.left * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Right))
        	    transform.position += Vector3.right * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Up))
               transform.position += Vector3.up * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Down))
        	    transform.position += Vector3.down * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.T))
                Debug.Log(transform.position);
        }
    }
}
