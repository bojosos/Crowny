using System;

namespace Crowny
{
    /// <summary>
    /// Controls which parts of an inspector value participate in a search.
    /// </summary>
    [Flags]
    public enum SearchFilterOptions
    {
        /// <summary>Does not match any built-in property data.</summary>
        None = 0,

        /// <summary>Matches the declared field or property name.</summary>
        PropertyName = 1 << 0,

        /// <summary>Matches the field or property name split into display words.</summary>
        PropertyNiceName = 1 << 1,

        /// <summary>Matches the value kind or declared type name.</summary>
        TypeOfValue = 1 << 2,

        /// <summary>Matches the text representation of a value.</summary>
        ValueToString = 1 << 3,

        /// <summary>Matches names, display names, types, and values.</summary>
        All = PropertyName | PropertyNiceName | TypeOfValue | ValueToString
    }

    /// <summary>
    /// Adds a search field that filters the children of an inspector value. Applying it to a type makes that type searchable wherever it is
    /// inspected.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property | AttributeTargets.Class | AttributeTargets.Struct, Inherited = true)]
    public sealed class Searchable : Attribute
    {
        /// <summary>
        /// Gets or sets the parts of each child that participate in the search.
        /// </summary>
        public SearchFilterOptions FilterOptions { get; set; } = SearchFilterOptions.All;

        /// <summary>
        /// Gets or sets whether non-contiguous, case-insensitive matches are accepted.
        /// </summary>
        public bool FuzzySearch { get; set; } = true;

        /// <summary>
        /// Gets or sets whether nested child values participate in the search.
        /// </summary>
        public bool Recursive { get; set; } = true;
    }
}
