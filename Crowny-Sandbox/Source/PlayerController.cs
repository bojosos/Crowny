using System;
using System.Collections.Generic;

using Crowny;

namespace Sandbox
{
    public class PlayerController : EntityBehaviour
    {
        private bool m_Grounded = true;
        private float m_HorizontalMove = 0f;
        private Vector3 m_Velocity = Vector3.zero;

        [ShowInInspector]
        private float m_MovementSmoothing = 0.05f;

        private Rigidbody2D m_Rigidbody;

        void Start()
        {
            m_Rigidbody = GetComponent<Rigidbody2D>();
        }

        void Update()
        {
            if (m_Grounded && Input.GetKeyDown(KeyCode.Space))
                m_Rigidbody.AddForce(Vector2.up * 10f);
            
            if (transform.position.y < -100) // restart
                transform.position = new Vector3(0, 2.56f, 0);

            if (Input.GetKey(KeyCode.Left))
                m_Rigidbody.AddForce(Vector2.left * 10);
            else if (Input.GetKey(KeyCode.Right))
                m_Rigidbody.AddForce(Vector2.right * 10);
        }
    }
}
