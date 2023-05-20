using System;

namespace Crowny
{
    /// <summary>
    /// EntityBehaviours marked with this attribute will get Start, Update, Destroy callbacks while in edit mode.
    /// </summary>
    [AttributeUsage(AttributeTargets.Class)]
    public class RunInEditorAttribute : Attribute
    {

    }
}