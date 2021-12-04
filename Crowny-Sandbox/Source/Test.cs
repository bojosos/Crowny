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

        [Crowny.Range(0.0f, 15.0f), ShowInInspector]
        private float speed = 1.0f;

        [ShowInInspector]
        private DrawMode drawMode = DrawMode.Quads;

        [ShowInInspector]
        private bool dummy = true;

        private float deltaSum = 0;
        private int fps = 0;
        private AudioSource source;

        public void Start()
        {
            if (!HasComponent<AudioListener>()) // Do [RequireComponent(AudioListener, AudioSource)] for the script at some point
                AddComponent<AudioListener>();
            if (!HasComponent<AudioSource>())
                source = AddComponent<AudioSource>();
            else
                source = GetComponent<AudioSource>();

            Matrix4 example = new Matrix4(new Vector4(1, 2, 3, 4), new Vector4(1, 2, 3, 4), new Vector4(1, 2, 3, 4), new Vector4(1, 2, 3, 4));
            Matrix4 result = example.inverse.inverse.inverse.inverse;
            Debug.Log("Matrix test is: \n\t" + example + "\nResult\n\t" + result);
            Debug.Log(example == result);
            Entity cur = Entity.FindByName("Test");
            int idx = 0;
            while (cur != null)
            {
                Debug.Log(idx);
                cur.Name = idx.ToString();
                cur = cur.parent;
                idx++;
            }
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
            deltaSum += Time.deltaTime;
            fps++;
            if (deltaSum > 1.0f)
            {
                Debug.Log(fps);
                fps = 0;
                deltaSum = 0;
                if (source.state == AudioSourceState.Playing)
                    source.Pause();
                else
                    source.Play();
            }
        }
    }
}
