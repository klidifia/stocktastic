(function ($) {
    Drupal.behaviors.stocktastic = {
        attach: function (context) {

            $("#table-filter").keyup(function () {
                // Split the words out of the filter box.
                var data = this.value.split(" ");
                var rows = $("#stocktastic-table tbody").find("tr");
                if (this.value == "") {
                    rows.show();
                    return;
                }
                rows.hide();

                // Recusively filter the rows object to get results.
                rows.filter(function (i, v) {
                    var $t = $(this);
                    for (var d = 0; d < data.length; ++d) {
                        if ($t.text().toLowerCase().indexOf(data[d].toLowerCase()) > -1) {
                            return true;
                        }
                    }
                    return false;
                })
                .show();
            });

            $(".stock").click(function (){
                $("#dialog p").css('margin-top', '20px');
                $("#dialog p").css('text-align', 'center');
                $("#dialog p").html('<img src="sites/all/themes/stocky/images/ajax-loader.gif" />');
                $.getJSON( "/js/stocktastic/getstockinfo/" + $(this).text(), function( data ) {
                    $(".ui-dialog-titlebar").show();
                    $(".ui-dialog-title").text(data.name);
                    $("#dialog p").css('margin-top', '0');
                    $("#dialog p").css('text-align', 'left');
                    var output = '<p>Price: $' + data.last_trade_price_only + '</p>';
                        output += '<p>Price start of year: $' + data.year_start_price + '</p>';
                        output += '<p>Dividends this year: $' + data.dividends + '</p>';
                        output += '<p>Manual adjustments: $' + data.manual_adjustments + '</p>';
                        output += '<p>Change this year: ' + data.year_percent_change + '%</p>';
                    $("#dialog p").html(output);
                });
                $("#dialog").dialog({ modal: true });
                $("#dialog").dialog({ width: 280 });
                $(".ui-dialog-titlebar").hide();
            ;});

        }
    };
})(jQuery);
