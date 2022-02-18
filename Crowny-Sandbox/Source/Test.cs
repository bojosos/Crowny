using System;

using Crowny;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {
        // Test range attr
        [Crowny.Range(0.0f, 15.0f), ShowInInspector]
        private float speed = 1.0f;

        // Test enums in editor
        public enum DrawMode
        {
            Traingles,
            TriangleStrip,
            Quads
        }
        [ShowInInspector]
        private DrawMode dummyEnumInspector = DrawMode.Quads;

        // Test bools
        [ShowInInspector]
        private bool dummyCheckboxInspector = true;

        // Test normal type inspector
        private float deltaSum = 0.69f; // This crashes the editor

        // This is wrong with new editor
        public int fps = 0;

        // Test audio
        private AudioSource source;

        private Test test;

        public Entity entityTest;

        public void Start()
        {
            // Test component stuff
            if (!HasComponent<AudioListener>()) // Do [RequireComponent(AudioListener, AudioSource)] for the script at some point
                AddComponent<AudioListener>();
            if (!HasComponent<AudioSource>())
                source = AddComponent<AudioSource>();
            else
                source = GetComponent<AudioSource>();

            // Test matrix stuff
            Matrix4 example = Matrix4.identity;
            example.SetColumn(3, new Vector4(3.0f, 2.0f, 5.0f, 1.0f));
            Debug.Log("Determinant: " + example.determinant);
            Matrix4 result = example.affineInverse;
            Debug.Log(result);
            Matrix4 reverser = result.affineInverse;
            Debug.Log("Matrix test is: \n\t" + example + "\nResult\n\t" + result);
            Debug.Log(example == result);

            // Rename all entities in the hierarchy above Test to 0-1-2-3....
            Entity cur = Entity.FindByName("Test");
            int idx = 0;
            while (cur != null)
            {
                Debug.Log(idx);
                cur.name = idx.ToString();
                cur = cur.parent;
                idx++;
            }
            // Test retrieve script component of another entity from this one. Soon will make it work as GetComponent<Test>();
            if (entity.parent != null)
            {
                EntityBehaviour script = entity.parent.GetComponent<EntityBehaviour>();
                Debug.Log(script);
                if (script != null)
                    test = script as Test;
            }
            if (entityTest != null)
                Debug.Log(entityTest.name);
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
                // Print out the fps counter of the parent
                if (test != null)
                    Debug.Log(test.fps); // Update order matters here
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
