using System;

using Crowny;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {
        public enum DrawMode
        {
            Traingles,
            TriangleStrip,
            Quads
        }

        [Crowny.Range(0.0f, 10.0f), ShowInInspector]
        private float speed = 1.0f;

        [ShowInInspector]
        private DrawMode drawMode = DrawMode.Quads;

        [ShowInInspector]
        private bool dummy = true;

        private Camera camera;

        private void TestFunc()
        {       
            int c = 0;
            int a = 5 / c;
        }

        public void Start()
        {
            TestFunc();
            int c = 0;
            int a = 5 / c;
//            camera = GetComponent<CameraComponent>().camera;
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
                Crowny.Debug.Log(transform.position);
        }
    }
}
