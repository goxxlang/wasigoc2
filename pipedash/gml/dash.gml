style {
}

view {
  div.app (
    import ("mast.gml")
    main.dash (
      aside#runlist.runs (
        h2 ("Runs")
        import ("runs.gml")
      )
      section.run (
        import ("head.gml")
        div#rail.rail (
          import ("stages.gml")
        )
        div.timeline (
          import ("timeline.gml")
        )
        div.figs (
          import ("headline.gml")
        )
        div.grid (
          div.card (
            h2 ("Gate metrics")
            div#metrics.scroller (
              import ("metrics.gml")
            )
          )
          div.col (
            div.card (
              h2 ("Findings and triage")
              import ("findings.gml")
            )
            div.card (
              h2 ("Flow log")
              import ("flowlog.gml")
            )
          )
        )
        div.grid (
          div.card (
            h2 ("Events")
            div#events.feed (
              import ("events.gml")
            )
          )
          div.card (
            h2 ("Decision layer")
            div.feed (
              import ("lessons.gml")
            )
          )
        )
      )
    )
  )
}
