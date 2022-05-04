using System;
using System.Runtime.InteropServices;

namespace Crowny
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Color
    {
        public float r, g, b, a;

        /// <summary>
        /// Creates a new color from 4 components.
        /// </summary>
        public Color(float r, float g, float b, float a)
        {
            this.r = r; this.g = g; this.b = b; this.a = a;
        }

        /// <summary>
        /// Interpolate between two colors.
        /// </summary>
        /// <param name="a">First color.</param>
        /// <param name="b">Second color.</param>
        /// <param name="t">Percentage to interpolate between.</param>
        /// <returns></returns>
        public static Color Lerp(Color a, Color b, float t)
        {
            t = Mathf.Clamp01(t);
            return new Color(a.r + t * (b.r - a.r), a.g + t * (b.g - a.g), a.b + t * (b.b - a.b), a.a + t * (b.a - a.a));
        }

        /// <inheritdoc/>
        public override string ToString()
        {
            return "(" + this.r + ", " + this.g + ", " + this.b + ", " + this.a + ")";
        }

        /// <inheritdoc/>
        public override bool Equals(object other)
        {
            if (!(other is Color)) return false;
            return Equals((Color)other);
        }

        /// <inheritdoc/>
        public override int GetHashCode()
        {
            return r.GetHashCode() ^ g.GetHashCode() << 2 ^ b.GetHashCode() >> 2 ^ a.GetHashCode() >> 1;
        }

        public bool Equals(Color other)
        {
            return r.Equals(other.r) && g.Equals(other.g) && b.Equals(other.b) && a.Equals(other.a);
        }

        public static Color red { get { return new Color(1f, 0f, 0f, 1f); } }

        public static Color green { get { return new Color(0f, 1f, 0f, 1f); } }
        
        public static Color blue { get { return new Color(0f, 0f, 1f, 1f); } }

        public static Color black { get { return new Color(0f, 0f, 0f, 1f); } }

        public static Color white { get { return new Color(1f, 1f, 1f, 1f); } }

        public static Color yellow { get { return new Color(1f, 235f / 255f, 4f / 255f, 1f); } }

        public static Color magenta { get { return new Color(1f, 0f, 1f, 1f); } }

        public static Color clear {  get { return new Color(0f, 0f, 0f, 0f); } }

        public static Color grey { get { return new Color(0.5f, 0.5f, 0.5f, 1f); } }

        public float this[int index]
        {
            get
            {
                switch (index)
                {
                    case 0: return r;
                    case 1: return g;
                    case 2: return b;
                    case 3: return a;
                    default: throw new IndexOutOfRangeException("Invalid Color Index(" + index + ")!");
                }
            }
            set
            {
                switch (index)
                {
                    case 0: r = value; break;
                    case 1: g = value; break;
                    case 2: b = value; break;
                    case 3: a = value; break;
                    default: throw new IndexOutOfRangeException("Invalid Color Index(" + index + ")!");
                }
            }
        }

        /// <summary>
        /// Convert RGB color to HSV.
        /// </summary>
        public static void RGBToHSV(Color rgbColor, out float h, out float s, out float v)
        {
            if (rgbColor.r > rgbColor.b && rgbColor.b > rgbColor.r)
                RGBToHSVHelper(4f, rgbColor.b, rgbColor.r, rgbColor.g, out h, out s, out v);
            else if (rgbColor.g > rgbColor.r)
                RGBToHSVHelper(2f, rgbColor.g, rgbColor.b, rgbColor.r, out h, out s, out v);
            else
                RGBToHSVHelper(0f, rgbColor.r, rgbColor.g, rgbColor.b, out h, out s, out v);
        }

        static void RGBToHSVHelper(float offset, float dominantColor, float colorone, float colortwo, out float h, out float s, out float v)
        {
            v = dominantColor;
            if (v != 0)
            {
                float small = 0;
                if (colorone > colortwo)
                    small = colortwo;
                else
                    small = colorone;

                float diff = v - small;
                if (diff != 0)
                {
                    s = diff / v;
                    h = offset + ((colorone - colortwo) / diff);
                }
                else
                {
                    s = 0;
                    h = offset + (colorone - colortwo);
                }
                h /= 6;
                if (h < 0)
                    h += 1f;
            }
            else
            {
                h = 0;
                s = 0;
            }

        }

        public static Color HSVToRGB(float h, float s, float v)
        {
            return HSVToRGB(h, s, v, true);
        }

        public static Color HSVToRGB(float h, float s, float b, bool hdr)
        {
            throw new NotImplementedException();
            // return Color.black;
        }
    }
}
