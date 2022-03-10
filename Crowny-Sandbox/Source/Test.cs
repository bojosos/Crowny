using System;
using System.Collections.Generic;

using Crowny;

namespace Sandbox
{
    public class Test : EntityBehaviour
    {
        public Test()
        {
            TestStructList.Add(new TestStruct());
            listOfBools.Add(false);
            listOfBools.Add(true);
            listOfInts.Add(69);
            listOfInts.Add(420);
        }
        // Test range attr
        [Crowny.Range(0.0f, 15.0f, true), ShowInInspector]
        private float speed = 1.0f;

        // Test enums in editor
        public enum DrawMode : uint
        {
            Traingles,
            TriangleStrip,
            Quads
        }
        [SerializeField]
        public DrawMode dummyEnumInspector = DrawMode.Quads;

        [ShowInInspector]
        private bool dummyCheckboxInspector = true;

        // Test normal type inspector
        [ShowInInspector]
        private float deltaSum = 0.69f;

        private int fpsAcc = 0;
        public int fps = 0;
        // Test audio
        private AudioSource source;

        private Test test;

        public Entity entityTest;

        public float Float = 14f;
        public double Double = 32;

        public sbyte Byte = 1;
        public byte UByte = 2;
        public short Short = 3;
        public ushort UShort = 4;
        public int Int = 5;
        public uint UInt = 6;
        public long Long = 7;
        public ulong ULong = 8;

        public string String = "Test123";
        public char Char = 'c';

        [Crowny.Range(0, 10, false)]
        public List<int> listOfInts = new List<int>();
        public List<bool> listOfBools = new List<bool>();

        public int Property { get; set; }

        [SerializeObject]
        public struct TestStruct
        {
            public TestStruct(int aa, int bb, int cc) { a = aa; b = bb; c = cc; }
            
            public int a, b, c;
            /*public int a { get; set; }
            public int b { get; set; }
            public int c { get; set; }*/
        }
        public TestStruct Struct = new TestStruct(1, 2, 3);
        public TestStruct DefaultStruct = new TestStruct();

        public List<TestStruct> TestStructList = new List<TestStruct>();

        [SerializeObject]
        public class TestClass
        {
            public TestClass(int a, int b, int c) { _a = a; _b = b; _c = c; }
            public int a { get { return _a; } set { _a = value; } }
            public int b { get { return _b; } set { _b = value; } }
            public int c { get { return _c; } set { _c = value; } }
            private int _a, _b, _c;
        }
        public TestClass testObj = new TestClass(31, 32, 33);
        public void Start()
        {
            Debug.Log("Length: " + listOfInts.Count);
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
            /*   if (entity.parent != null)
               {
                   EntityBehaviour script = entity.parent.GetComponent<EntityBehaviour>();
                   Debug.Log(script);
                   if (script != null)
                       test = script as Test;
               }*/
            if (entityTest != null)
                Debug.Log(entityTest.name);
        }

        public void Update()
        {
            Struct.a++;
            Struct.b--;
            if (Input.GetKey(KeyCode.Left))
                transform.position += Vector3.left * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Right))
                transform.position += Vector3.right * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Up))
                transform.position += Vector3.up * speed * Time.smoothDeltaTime;
            if (Input.GetKey(KeyCode.Down))
                transform.position += Vector3.down * speed * Time.smoothDeltaTime;
            deltaSum += Time.deltaTime;
            fpsAcc++;
            if (deltaSum > 1.0f)
            {
                // Print out the fps counter of the parent
                // if (test != null)
                // Debug.Log(test.fps); // Update order matters here
                fps = fpsAcc;
                fpsAcc = 0;
                deltaSum = 0;
                if (source.state == AudioSourceState.Playing)
                    source.Pause();
                else
                    source.Play();
            }
        }
    }
}
